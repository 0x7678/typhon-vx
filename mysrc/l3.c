#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <sqlite3.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/types.h>  // on some older debian versions (backtrack?)
#include <sys/stat.h>   // theese two needs to be included

#include <osmocom/core/msgb.h>
#include <osmocom/gsm/rsl.h>
#include <osmocom/gsm/tlv.h>
#include <osmocom/gsm/gsm48_ie.h>
#include <osmocom/gsm/gsm48.h>
#include <osmocom/gsm/protocol/gsm_04_08.h>

#include <osmocom/bb/common/logging.h>
#include <osmocom/bb/misc/rslms.h>
#include <osmocom/bb/misc/layer3.h>
#include <osmocom/bb/common/osmocom_data.h>
#include <osmocom/bb/common/l1ctl.h>
#include <osmocom/bb/common/sysinfo.h>


#include "gsmstack.h"
#include "custom_ms.h"
#include "common.h"

#define empty_key "0000000000000000"

extern GS_CTX d_gs_ctx_downlink;
extern GS_CTX d_gs_ctx_downlink_crypt;
extern GS_CTX d_gs_ctx_uplink;
extern GS_CTX d_gs_ctx_uplink_crypt;
extern unsigned char d_KC[];
extern unsigned char mytmsi[];

extern uint32_t b_mcnc;
extern uint32_t b_cellid;

#define L3_MSG_HEAD 4
#define L3_MSG_SIZE (sizeof(struct l1ctl_hdr)+sizeof(struct l1ctl_info_dl)+sizeof(struct l1ctl_burst_ind) + L3_MSG_HEAD)


int static downlink_cntr = 0;
int static uplink_cntr = 0;

unsigned char d_KC_none[8] = {0, 0, 0, 0, 0, 0, 0, 0};

assignment transport;
app_state_t app_state;

int comm;

int rxlev_total=0;
int rxlev_samples=0;

uint8_t prev_chan_nr = 0;
struct timespec ass_now, ass_then;
#define assignment_guard_period 100
double timediff;

void sig_alrm(int signo) {
	printf("Failed to sync (timeout)\n");
	exit(31);
}

int release(struct osmocom_ms *ms) {

	struct custom_ms *cms = get_custom_ms(ms);

	/* L1 release */
	l1ctl_tx_dm_rel_req(ms);

	if (mode == MODE_SLAVE) {
		/* nothing */
	} else {
		// return to BCCH
		l1ctl_tx_fbsb_req(ms, ms->test_arfcn,
			L1CTL_FBSB_F_FB01SB, 100, 0,
			app_state.ccch_mode);
	}

	/* Change state */
	app_state.dch_state = DCH_WAIT_REL;
	app_state.dch_badcnt = 0;

	/* Close output */
	if (app_state.fh) {
		fclose(app_state.fh);
		app_state.fh = NULL;
	}

	sqlite3 *conn;
	sqlite3_stmt    *res;
	int     sqlerr = 0;
	int     rec_count = 0;
	const char      *errMSG;
	const char      *tail;

	char tmsi2burstsfile[PATH_MAX];
	char query[80];
	sprintf(tmsi2burstsfile, "%s/keys.db", iniparser_getstring(ini, "Main:GSMSESSION", NULL));


	if (strlen(cms->dat_file) > 0) {
		sqlerr = sqlite3_open(tmsi2burstsfile, &conn);
		if (sqlerr) {
			printf("Can not open database err = %i\n", sqlerr);
			exit(42);
		}
		// XXX MCNC, CELLID
		int rxlev_avg = 0;
		if(rxlev_samples>0) { // avoid division by zero
			rxlev_avg = rxlev_total/rxlev_samples;
		}
		sprintf(query, "INSERT INTO keys VALUES('%i','%s','%08x','%s',0,%d,%d,%i)", time(NULL), cms->dat_file, ntohl(cms->tmsi), empty_key,b_mcnc,b_cellid,rxlev_avg);
		sqlerr = SQLITE_OK;
		do {
			sqlerr = sqlite3_exec(conn, query, 0, 0, 0);
			usleep(SQL_RETRY_TIME);
		} while (sqlerr != SQLITE_OK);
	
		rxlev_samples=0;
		rxlev_total=0;

		sqlerr = sqlite3_close(conn);
	}

	if (mode == MODE_SLAVE) {
		// reset and wait for next job
		//l1ctl_tx_reset_req(ms, L1CTL_RES_T_FULL);
		int fd,size_r;
		printf("DBG: Slave_release\n");   
		l1ctl_tx_fbsb_req(ms, ms->test_arfcn, L1CTL_FBSB_F_FB01SB, 100, 0, app_state.ccch_mode);

		fd = open(pool_pipe, O_RDWR);

		size_r=read(fd, &transport, sizeof(transport));
		close(fd);

		if (size_r != sizeof(transport)) {
			printf("Error: read %d instead of %d bytes, ignoring.\n",size_r,sizeof(transport));
			return -1;
		}
		assert(transport.kanarek == 0xdefeca7e);
		printf("DBG: read: sync_arfcn: %d\n", transport.sync_arfcn);
		ms->test_arfcn = transport.sync_arfcn;
		b_mcnc=transport.b_mcnc;
		b_cellid=transport.b_cellid;

		// fixed: do not reuse GLOBAL structure between bursts
		destroy_custom_ms(ms);
		init_custom_ms(ms);

		signal(SIGALRM, sig_alrm);
		alarm(1);
	}

	return 0;
}

void get_key(uint32_t tmsi) {
/*	char cmd[PATH_MAX];
	FILE* f;
	char buf[17];

	sprintf(cmd,"%s/mysrc/tmsi2key %08x", iniparser_getstring(ini, "Main:GSMPATH", NULL), ntohl(tmsi));
	f=popen(cmd,"r");
	fread(buf, 16, 1, f);
	pclose(f);*/


	sqlite3 *conn;
	sqlite3_stmt    *res;
	int     sqlerr = 0;
	int     rec_count = 0;
	const char      *errMSG;
	const char      *tail;

	char buf[17];
	char tmsi2burstsfile[PATH_MAX];
	char query[120];

//	fprintf(stderr, "GET_KEY\n");

	sprintf(tmsi2burstsfile, "%s/keys.db", iniparser_getstring(ini, "Main:GSMSESSION", NULL));

	sqlerr = sqlite3_open(tmsi2burstsfile, &conn);
	if (sqlerr) {
		printf("Can not open database err = %i\n", sqlerr);
		exit(42);
	}

	sprintf(query, "SELECT key FROM keys WHERE key != '%s' AND tmsi = '%08x' ORDER BY timestamp DESC LIMIT 1", empty_key, ntohl(tmsi));
	sqlerr = SQLITE_OK;
	do {
        	sqlerr = sqlite3_prepare_v2(conn, query, 1000, &res, &tail);
//		printf("sql_exec = %i\n", sqlerr);
		usleep(1000);
	} while (sqlerr != SQLITE_OK);


        if(SQLITE_ROW == sqlite3_step(res)) {
	        snprintf(buf, 17, sqlite3_column_text(res, 0));
		buf[16] = 0;
		fprintf(stderr, "KEY FOUND: %s\n", buf);
	} else {
		memcpy(buf, "0000000000000000\0", 17);
//		fprintf(stderr, "KEY NOT FOUND: %s\n", buf);
	}


	sqlite3_finalize(res);
	sqlerr = sqlite3_close(conn);
//	printf("sql_close = %i\n", sqlerr);

	set_key(buf);
}

int print_mi(struct osmocom_ms *ms, uint8_t *mi, int num)
{
	uint8_t mi_type;
	char imsi[16];
	struct custom_ms *cms = get_custom_ms(ms);

        mi_type = mi[1] & GSM_MI_TYPE_MASK;
        switch (mi_type) {
                case GSM_MI_TYPE_TMSI:
			memcpy(&cms->tmsi, mi+2, 4);
			printf("TMSI-%i %08x\n", num, ntohl(cms->tmsi));
			if(!(mytmsi[0] & mytmsi[1] & mytmsi[2] &mytmsi[3]))
				get_key(cms->tmsi);
				return 0;
			if(memcmp(&cms->tmsi, mytmsi, 4))
				return 0;
			return 1;
                        break;
                case GSM_MI_TYPE_IMSI:
                        gsm48_mi_to_string(imsi, sizeof(imsi), mi + 1, mi[0]);
                        printf("IMSI-%i %s\n", num, imsi);
			return 0;
                        break;
                case GSM_MI_TYPE_NONE:
                        printf("MI type NONE\n");
                        break;
                default:
                        printf("Unknown MI type\n");
        }
	return 0;
}

static int gsm48_rx_pag_resp(struct msgb *msg, struct osmocom_ms *ms)
{
	struct gsm48_pag_rsp *pr = msgb_l3(msg);
	uint8_t *mi;

	mi = pr->data + 2;		/* ??? */
	printf("PR MI=");
	if (print_mi(ms, mi, 1)) {
		printf("RELEASE gsm48_rx_pag_resp\n\n");
		release(ms);
	}

}

static int gsm48_rx_loc_upd_req(struct msgb *msg, struct osmocom_ms *ms)
{
	struct gsm48_loc_upd_req *lur = msgb_l3(msg);
	uint8_t *mi;

	mi = lur->mi + 1;		/* ??? */
	printf("LUR MI=");
	if (print_mi(ms, mi, 1)) {
		printf("RELEASE gsm48_rx_loc_upd_req\n\n");
		release(ms);
	}
}

static int gsm48_rx_cm_serv_req(struct msgb *msg, struct osmocom_ms *ms)
{
	struct gsm48_service_request *csr = msgb_l3(msg);
	uint8_t *mi;

	mi = csr->mi + 1;		/* ??? */
	printf("CSR MI=");
	if (print_mi(ms, mi, 1)) {
		printf("RELEASE gsm48_rx_cm_serv_req\n\n");
		release(ms);
	}
}

static int gsm48_rx_imsi_detach(struct msgb *msg, struct osmocom_ms *ms)
{
	struct gsm48_service_request *id = msgb_l3(msg);
	uint8_t *mi;
	int i;

	mi = (uint8_t *)(id->mi + 4);			/* ??? */
	printf("ID DUMP: ");
	for(i=0; i<23; i++)
		printf("%.2x ", ((unsigned char *)id)[i]);
	printf("MI DUMP: ");
	hexdump((char *) mi, 6);
	printf("\nID MI=");
	if (print_mi(ms, mi, 1)) {
		printf("RELEASE gsm48_rx_cm_serv_req\n\n");
		release(ms);
	}
}

/* render list of hopping channels from channel description elements */
static int gsm48_rr_render_ma(struct osmocom_ms *ms, struct gsm48_rr_cd *cd,
	uint16_t *ma, uint8_t *ma_len)
{
	struct gsm48_sysinfo *s = ms->cellsel.si;
	struct gsm_support *sup = &ms->support;
	int i;
	uint16_t arfcn;

	/* no hopping (no MA), channel description is valid */
	if (!cd->h) {
		ma_len = 0;
		return 0;
	}

	/* decode mobile allocation */
	if (cd->mob_alloc_lv[0]) {
		struct gsm_sysinfo_freq *freq = s->freq;
	
		LOGP(DRR, LOGL_INFO, "decoding mobile allocation\n");

		if (cd->cell_desc_lv[0]) {
			LOGP(DRR, LOGL_INFO, "using cell channel descr.\n");
			if (cd->cell_desc_lv[0] != 16) {
				LOGP(DRR, LOGL_ERROR, "cell channel descr. "
					"has invalid lenght\n");
				return GSM48_RR_CAUSE_ABNORMAL_UNSPEC;
			}
			gsm48_decode_freq_list(freq, cd->cell_desc_lv + 1, 16,
				0xce, FREQ_TYPE_SERV);
		}

		gsm48_decode_mobile_alloc(freq, cd->mob_alloc_lv + 1,
			cd->mob_alloc_lv[0], ma, ma_len, 0);
		if (*ma_len < 1) {
			LOGP(DRR, LOGL_NOTICE, "mobile allocation with no "
				"frequency available\n");
			return GSM48_RR_CAUSE_NO_CELL_ALLOC_A;

		}
	} else
	/* decode frequency list */
	if (cd->freq_list_lv[0]) {
		struct gsm_sysinfo_freq f[1024];
		int j = 0;

		LOGP(DRR, LOGL_INFO, "decoding frequency list\n");

		/* get bitmap */
		if (gsm48_decode_freq_list(f, cd->freq_list_lv + 1,
			cd->freq_list_lv[0], 0xce, FREQ_TYPE_SERV)) {
			LOGP(DRR, LOGL_NOTICE, "frequency list invalid\n");
			return GSM48_RR_CAUSE_ABNORMAL_UNSPEC;
		}

		/* collect channels from bitmap (1..1023,0) */
		for (i = 1; i <= 1024; i++) {
			if ((f[i & 1023].mask & FREQ_TYPE_SERV)) {
				LOGP(DRR, LOGL_INFO, "Listed ARFCN #%d: %d\n",
					j, i);
				if (j == MA_MAX_LEN) {
					LOGP(DRR, LOGL_NOTICE, "frequency list "
						"exceeds %d entries!\n",MA_MAX_LEN);
					return GSM48_RR_CAUSE_ABNORMAL_UNSPEC;
				}
				ma[j++] = i;
			}
		}
		*ma_len = j;
		return 0;
	} else
	/* decode frequency channel sequence */
	if (cd->freq_seq_lv[0]) {
		int j = 0, inc;

		LOGP(DRR, LOGL_INFO, "decoding frequency channel sequence\n");

		if (cd->freq_seq_lv[0] != 9) {
			LOGP(DRR, LOGL_NOTICE, "invalid frequency channel "
				"sequence\n");
			return GSM48_RR_CAUSE_ABNORMAL_UNSPEC;
		}
		arfcn = cd->freq_seq_lv[1] & 0x7f;
		LOGP(DRR, LOGL_INFO, "Listed Sequence ARFCN #%d: %d\n", j,
			arfcn);
		ma[j++] = arfcn;
		for (i = 0; i <= 16; i++) {
			if ((i & 1))
				inc = cd->freq_seq_lv[2 + (i >> 1)] & 0x0f;
			else
				inc = cd->freq_seq_lv[2 + (i >> 1)] >> 4;
			if (inc) {
				arfcn += inc;
				LOGP(DRR, LOGL_INFO, "Listed Sequence ARFCN "
					"#%d: %d\n", j, arfcn);
				ma[j++] = arfcn;
			} else
				arfcn += 15;
		}
		*ma_len = j;
		return 0;
	} else {
		LOGP(DRR, LOGL_NOTICE, "hopping, but nothing that tells us "
			"a sequence\n");
		return GSM48_RR_CAUSE_ABNORMAL_UNSPEC;
	}

	return 0;
}

//  imm_ass je ten prvni skok z broadcast channelu a ass_cmd ten druhy, na hovor
static int gsm48_rr_rx_ass_cmd(struct msgb *msg, struct osmocom_ms *ms)
{
	struct gsm48_rrlayer *rr = &ms->rrlayer;
        struct gsm48_hdr *gh = msgb_l3(msg);
        struct gsm48_ass_cmd *ac = (struct gsm48_ass_cmd *)gh->data;
	uint8_t ch_type, ch_subch, ch_ts;
	int rv, res;

	printf("gsm48_rr_rx_ass_cmd\n");  

	rsl_dec_chan_nr(ac->chan_desc.chan_nr, &ch_type, &ch_subch, &ch_ts);
	d_gs_ctx_uplink_crypt.ts_ctx[ch_ts].type = TST_TCHF;
	d_gs_ctx_downlink_crypt.ts_ctx[ch_ts].type = TST_TCHF;


	if (!ac->chan_desc.h0.h) {
		/* Non-hopping */
		uint16_t arfcn;

		arfcn = ac->chan_desc.h0.arfcn_low | (ac->chan_desc.h0.arfcn_high << 8);

		printf("GSM48 ASS CMD(chan_nr=0x%02x, "
			"ARFCN=%u, TS=%u, SS=%u, TSC=%u) ",
			ac->chan_desc.chan_nr, arfcn, ch_ts, ch_subch,
			ac->chan_desc.h0.tsc);


		/* request L1 to go to dedicated mode on assigned channel */
		if(mode == MODE_MASTER) { //master
			transport.arfcn = arfcn;
			transport.chan_nr = ac->chan_desc.chan_nr;
			transport.tsc = ac->chan_desc.h0.tsc; // TODO: continue
			transport.b_mcnc=b_mcnc;
			transport.b_cellid=b_cellid;
			transport.kanarek=0xdefeca7e;

			printf("JUMP:409 FIXME\n");
			return(42);
		}
		rv = l1ctl_tx_dm_est_req_h0(ms,
			arfcn, ac->chan_desc.chan_nr, ac->chan_desc.h0.tsc,
			GSM48_CMODE_SPEECH_EFR, 0);
	} else {
		/* Hopping */
		uint8_t maio, hsn, ma_len;
		uint16_t ma[MA_MAX_LEN], arfcn;
		int i, j, k;
		struct gsm48_rr_cd *cda = &rr->cd_after;
		int payload_len = msgb_l3len(msg) - sizeof(*gh) - sizeof(*ac);
		struct tlv_parsed tp;
		const uint8_t *lv;
		const uint8_t *v;
		uint8_t len;


		hsn = ac->chan_desc.h1.hsn;
		maio = ac->chan_desc.h1.maio_low | (ac->chan_desc.h1.maio_high << 2);

		printf("GSM48 ASS CMD(chan_nr=0x%02x, "
			"HSN=%u, MAIO=%u, TS=%u, SS=%u, TSC=%u) ",
			ac->chan_desc.chan_nr, hsn, maio, ch_ts, ch_subch,
			ac->chan_desc.h1.tsc);

		tlv_parse(&tp, &gsm48_rr_att_tlvdef, ac->data, payload_len, 0, 0);
		tlv_dump(&tp);

		if (TLVP_PRESENT(&tp, GSM48_IE_MA_AFTER)) {
			lv = TLVP_VAL(&tp, GSM48_IE_MA_AFTER) - 1;
			memcpy(cda->mob_alloc_lv, lv, *lv + 1);
		}

		if (TLVP_PRESENT(&tp, GSM48_IE_CELL_CH_DESC)) {
			v = TLVP_VAL(&tp, GSM48_IE_CELL_CH_DESC);
			len = TLVP_LEN(&tp, GSM48_IE_CELL_CH_DESC);
			cda->cell_desc_lv[0] = len;
			memcpy(cda->cell_desc_lv + 1, v, len);
		}

		if (TLVP_PRESENT(&tp, GSM48_IE_FREQ_L_AFTER)) {
			lv = TLVP_VAL(&tp, GSM48_IE_FREQ_L_AFTER) - 1;
			memcpy(cda->freq_list_lv, lv, *lv + 1);
		}

		if (TLVP_PRESENT(&tp, GSM48_IE_CHANMODE_1)) {
			cda->mode = *TLVP_VAL(&tp, GSM48_IE_CHANMODE_1);
		}

		cda->h = 1;

		ms->cellsel.si = malloc(sizeof(struct gsm48_sysinfo));

		/* decode mobile allocation */
		res = gsm48_rr_render_ma(ms, cda, ma, &ma_len);
		if (res) {
			printf("gsm48_rr_render_ma error=%i\n", res);
			return;
		}

//		/* request L1 to go to dedicated mode on assigned channel */
		printf("JUMP:472 FIXME\n"); return(42);
		rv = l1ctl_tx_dm_est_req_h1(ms,
			maio, hsn, ma, ma_len,
			ac->chan_desc.chan_nr, ac->chan_desc.h1.tsc,
			GSM48_CMODE_SPEECH_AMR,0);
	}
}

static int gsm48_rx_imm_ass(struct msgb *msg, struct osmocom_ms *ms)
{
	struct gsm48_imm_ass *ia = msgb_l3(msg);
	uint8_t ch_type, ch_subch, ch_ts;
	int rv;

	/* Discard packet TBF assignement */
	if (ia->page_mode & 0xf0)
		return 0;

	/* If we're busy ... */
	if (app_state.dch_state != DCH_NONE)
		return 0;

	/* FIXME: compare RA and GSM time with when we sent RACH req */

	rsl_dec_chan_nr(ia->chan_desc.chan_nr, &ch_type, &ch_subch, &ch_ts);

	if (!ia->chan_desc.h0.h) {
		/* Non-hopping */
		uint16_t arfcn;

		arfcn = ia->chan_desc.h0.arfcn_low | (ia->chan_desc.h0.arfcn_high << 8);

		DEBUGP(DRR, "GSM48 IMM ASS (chan_nr=0x%02x, "
			"ARFCN=%u, TS=%u, SS=%u, TSC=%u) ",
			ia->chan_desc.chan_nr, arfcn, ch_ts, ch_subch,
			ia->chan_desc.h0.tsc);

		/* request L1 to go to dedicated mode on assigned channel */
//		printf("mode: %x\n", mode);
		if(mode == MODE_MASTER) { // master
			clock_gettime( CLOCK_REALTIME, &ass_now);

			/* Sometimes BTS sends immediate assignment to the same channel multiple times.	Namely, this has been observed on T-Mobile CZ network (23001).
			Original behavior is of course to follow all assignments we observe, however, in case of multiple assignments to the same channel, we will record the same communication by multiple slave phones. As a workaround, we will remember channel number of the last immediate assignment and will not follow any subsequent assignments to that channel in the next assignment_guard_period miliseconds.
			Even this sometimes fails - when there is another assignment inserted inbetween these same two. But it happens very rarely. Better workaround would be to remember the last channel numbers and timestamps when they were received. Solution would be not to send more slaves to a channel, where one slave is already listening. */
			if(prev_chan_nr == ia->chan_desc.chan_nr) { // possible duplicate assignment
				timediff = ( ass_now.tv_sec - ass_then.tv_sec )*1000 + (double)( ass_now.tv_nsec - ass_then.tv_nsec ) / (double)1000000;
				printf("Two IMM_ASS to same channel, delta=%3f\n",timediff);
				if(timediff < assignment_guard_period) {
					printf("Dropping frame chan_nr: 0x%x, arfcn: %d, tsc: 0x%x!\n", ia->chan_desc.chan_nr, arfcn, ia->chan_desc.h0.tsc);
					return(44);
				}
			}
			clock_gettime( CLOCK_REALTIME, &ass_then);
			prev_chan_nr = ia->chan_desc.chan_nr;
			printf("JUMP chan_nr: 0x%x, arfcn: %d, tsc: 0x%x ...", ia->chan_desc.chan_nr, arfcn, ia->chan_desc.h0.tsc);
			transport.ass_type = 0;
			transport.arfcn = arfcn;
			transport.sync_arfcn = ms->test_arfcn;
			transport.chan_nr = ia->chan_desc.chan_nr;
			transport.tsc = ia->chan_desc.h0.tsc;
			transport.tmsi = 0;
			transport.b_mcnc=b_mcnc;
			transport.b_cellid=b_cellid;
			transport.kanarek=0xdefeca7e;

			comm = open(pool_pipe, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			write(comm, (char *) &transport, sizeof(transport));
			close(comm);
			printf("OK\n");
			return(43);
		}

		printf("JUMP: Standalone, 0, chan_nr: %x, arfcn: %d, tsc: %d\n",ia->chan_desc.chan_nr, ms->test_arfcn, ia->chan_desc.h0.tsc);
		rv = l1ctl_tx_dm_est_req_h0(ms,
			arfcn, ia->chan_desc.chan_nr, ia->chan_desc.h0.tsc,
			GSM48_CMODE_SIGN, 0);
	} else {
		/* Hopping */
		uint8_t maio, hsn, ma_len;
		uint16_t ma[MA_MAX_LEN], arfcn;
		int i, j, k;

		hsn = ia->chan_desc.h1.hsn;
		maio = ia->chan_desc.h1.maio_low | (ia->chan_desc.h1.maio_high << 2);

		DEBUGP(DRR, "GSM48 IMM ASS (chan_nr=0x%02x, "
			"HSN=%u, MAIO=%u, TS=%u, SS=%u, TSC=%u) ",
			ia->chan_desc.chan_nr, hsn, maio, ch_ts, ch_subch,
			ia->chan_desc.h1.tsc);

		/* decode mobile allocation */
		ma_len = 0;
		for (i=1, j=0; i<=1024; i++) {
			arfcn = i & 1023;
			if (app_state.cell_arfcns[arfcn].mask & 0x01) {
				k = ia->mob_alloc_len - (j>>3) - 1;
				if (ia->mob_alloc[k] & (1 << (j&7))) {
					ma[ma_len++] = arfcn;
				}
				j++;
			}
		}

		/* request L1 to go to dedicated mode on assigned channel */

		if(mode == MODE_MASTER) { // master

			printf("JUMP maio: 0x%x, hsn: 0x%x, ma: 0x%x, ma_len: %d, chan_nr: 0x%x, tsc: 0x%x ...", maio, hsn, ma, ma_len,ia->chan_desc.chan_nr, ia->chan_desc.h1.tsc);
			transport.ass_type = 1;
			transport.sync_arfcn = ms->test_arfcn;
			transport.chan_nr = ia->chan_desc.chan_nr;
			transport.tsc = ia->chan_desc.h0.tsc;
			transport.maio = maio;
			transport.hsn = hsn;
			transport.ma_len = ma_len;
			transport.tmsi = 0;
			transport.b_mcnc=b_mcnc;
			transport.b_cellid=b_cellid;
			transport.kanarek=0xdefeca7e;
			memcpy(transport.ma,ma,ma_len*sizeof(uint16_t));

			comm = open(pool_pipe, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			write(comm, (char *) &transport, sizeof(transport));
			close(comm);
			printf("OK\n");
			return(44);
		}

		printf("JUMP: Standalone, 1, chan_nr: %x, tsc: %d\n", ia->chan_desc.chan_nr, ia->chan_desc.h0.tsc);
		rv = l1ctl_tx_dm_est_req_h1(ms,
			maio, hsn, ma, ma_len,
			ia->chan_desc.chan_nr, ia->chan_desc.h1.tsc,
			GSM48_CMODE_SIGN, 0);
	}

	DEBUGPC(DRR, "\n");

	     /* Set state */
	app_state.dch_state = DCH_WAIT_EST;
	app_state.dch_nr = ia->chan_desc.chan_nr;
	app_state.dch_badcnt = 0;

	return rv;
}

int gsm48_rx_ccch(struct msgb *msg, struct osmocom_ms *ms)
{
	struct gsm48_system_information_type_header *sih = msgb_l3(msg);
	int rc = 0;

	/* CCCH marks the end of WAIT_REL */
	if (app_state.dch_state == DCH_WAIT_REL)
		app_state.dch_state = DCH_NONE;

	if (sih->rr_protocol_discriminator != GSM48_PDISC_RR)
		LOGP(DRR, LOGL_ERROR, "PCH pdisc != RR\n");

	switch (sih->system_information) {
	case GSM48_MT_RR_PAG_REQ_1:
	case GSM48_MT_RR_PAG_REQ_2:
	case GSM48_MT_RR_PAG_REQ_3:
		/* FIXME: implement decoding of paging request */
		break;
	case GSM48_MT_RR_IMM_ASS:
		rc = gsm48_rx_imm_ass(msg, ms);
		break;
	default:
		LOGP(DRR, LOGL_NOTICE, "unknown PCH/AGCH type 0x%02x\n",
			sih->system_information);
		rc = -EINVAL;
	}

	return rc;
}

int gsm48_rx_bcch(struct msgb *msg, struct osmocom_ms *ms)
{
	/* BCCH marks the end of WAIT_REL */
	if (app_state.dch_state == DCH_WAIT_REL)
		app_state.dch_state = DCH_NONE;

	return 0;
}

static char *
gen_filename(struct osmocom_ms *ms, struct l1ctl_burst_ind *bi)
{
	static char buffer[PATH_MAX];
	time_t d;
	struct tm lt;

	time(&d);
	localtime_r(&d, &lt);

	snprintf(buffer, PATH_MAX, "bursts_%04d%02d%02d_%02d%02d%02d_%i_%d_%02x_%d.dat",
		lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
		lt.tm_hour, lt.tm_min, lt.tm_sec,
		// only some bits of band_arfcn contains arfcn itself
		ntohs(bi->band_arfcn) & (~ARFCN_UPLINK),
		ntohl(bi->frame_nr),
		bi->chan_nr,
		(int) getpid()
	);

	return buffer;
}

void layer3_rx_burst(struct osmocom_ms *ms, struct msgb *msg)
{
	struct l1ctl_burst_ind *bi;
	int16_t rx_dbm;
	uint16_t arfcn;
	int ul, do_rel=0;

	uint8_t chan_nr, chan_type, chan_ts, chan_ss;
	uint8_t gsmtap_chan_type;
	struct gsm_time tm;

	int res, res2;
	struct msgb *msg2;
	struct l1ctl_info_dl *dl;
	struct l1ctl_data_ind *di;
	struct l1ctl_hdr *l1h;


	/* Header handling */
	bi = (struct l1ctl_burst_ind *) msg->l1h;

	arfcn = ntohs(bi->band_arfcn);
	rx_dbm = rxlev2dbm(bi->rx_level);
	ul = !!(arfcn & ARFCN_UPLINK);

        rsl_dec_chan_nr(bi->chan_nr, &chan_type, &chan_ss, &chan_ts);
	gsm_fn2gsmtime(&tm, ntohl(bi->frame_nr));

	if (ul) {
		res=GS_process(&d_gs_ctx_uplink, bi, d_KC_none, !uplink_cntr, 0);
		res2=GS_process(&d_gs_ctx_uplink_crypt, bi, d_KC, !uplink_cntr, 0);
		uplink_cntr++;
		uplink_cntr = uplink_cntr % 4;
	} else {
		res=GS_process(&d_gs_ctx_downlink, bi, d_KC_none, !downlink_cntr, 0);
		res2=GS_process(&d_gs_ctx_downlink_crypt, bi, d_KC, !downlink_cntr, 0);
		downlink_cntr++;
		downlink_cntr = downlink_cntr % 4;
		if(res2 > 0) {
			res=1;
			memcpy(&d_gs_ctx_downlink.msg, &d_gs_ctx_downlink_crypt.msg, 23);
		}
		if(res > 0) {
			msg2 = msgb_alloc_headroom(L3_MSG_SIZE, L3_MSG_HEAD, "l1ctl");
			l1h = (struct l1ctl_hdr *) msgb_put(msg2, sizeof(*l1h));
			l1h->msg_type = L1CTL_DATA_IND;
			l1h->flags = 0;
			msg2->l1h = (uint8_t *)l1h;

			dl = (struct l1ctl_info_dl *) msgb_put(msg2, sizeof(*dl));
			di = (struct l1ctl_data_ind *) msgb_put(msg2, sizeof(*di));
			dl->chan_nr = bi->chan_nr;
			if (bi->flags & BI_FLG_SACCH)
				dl->link_id = 0x40;
			else
				dl->link_id = 0x00;
			dl->band_arfcn = bi->band_arfcn;
			dl->frame_nr = bi->frame_nr;
			dl->snr = bi->snr;
			rxlev_samples++;
			rxlev_total+=(int)bi->rx_level-110;
			dl->rx_level = bi->rx_level;
			dl->num_biterr = 0;
			msg2->l2h = (uint8_t *)dl;

			memcpy(di, &d_gs_ctx_downlink.msg, 23);

			
			if (di->data[1] & 0x01) {
				di->data[0] = 0x03;				/* Bypass lapd processing */
				di->data[1] = 0x03;
			} else {
				ms->lapdm_channel.lapdm_dcch.datalink->dl.state = LAPD_STATE_MF_EST;
				ms->lapdm_channel.lapdm_dcch.datalink->dl.v_recv = (((di->data[1]) & 0xE) >> 1);
				ms->lapdm_channel.lapdm_dcch.datalink->dl.v_ack = ((((di->data[1]) & 0xE) >> 1) + 1) % 8;
			}

			msg2->l3h = (uint8_t *)di;

			int i;
//			printf("Decoded: ");
//			for(i=0;i<23;i++)
//				printf("%02x ", di->data[i]);
//			printf("\n");
			res=l1ctl_recv(ms, msg2);
//			printf("Inject message %i\n", res);
		}
	}


	/* Check for channel start */
	if (app_state.dch_state == DCH_WAIT_EST) {
		if (bi->chan_nr == app_state.dch_nr) {
			if (bi->snr > 64) {
				char datfile[PATH_MAX];
				/* Change state */
				app_state.dch_state = DCH_ACTIVE;
				app_state.dch_badcnt = 0;

				/* Open output */
				struct custom_ms *cms = get_custom_ms(ms);
				strcpy(cms->dat_file, gen_filename(ms, bi));
				printf("Filename: %s\n", cms->dat_file);
				sprintf(datfile, "%s", cms->dat_file);
//				sprintf(datfile, "%s/%s", iniparser_getstring(ini, "Main:GSMSESSION", NULL), cms->dat_file);

				app_state.fh = fopen(datfile, "wb");
			} else {
				/* Abandon ? */
				do_rel = (app_state.dch_badcnt++) >= CHAN_EARLY_ABANDON_CNT;
				//printf("Abandon=%i\n",app_state.dch_badcnt);
			}
		}
	}

	/* Check for channel end */
	if (app_state.dch_state == DCH_ACTIVE) {
		if (!ul) {
			/* Bad burst counting */
			if (bi->snr < 64)
				app_state.dch_badcnt++;
			else if (app_state.dch_badcnt >= 2)
				app_state.dch_badcnt -= 2;
			else
				app_state.dch_badcnt = 0;

			/* Release condition */
			do_rel = app_state.dch_badcnt >= BADCOUNT_RELEASE/*60*/;
		}
	}

	/* Release ? */
	if (do_rel) {
		printf("RELEASE snr\n\n");
		release(ms);
	}

	/* Save the burst */
//	printf("(%6d = %.4u/%.2u/%.2u)", tm.fn, tm.t1, tm.t2, tm.t3);
//	printf("(%4d dBm, SNR %3d%s%s) ", (int)bi->rx_level-110, bi->snr, arfcn & ARFCN_UPLINK ? ", UL" : "", bi->flags & BI_FLG_SACCH ? ", SACCH" : "");

	if (app_state.dch_state == DCH_ACTIVE) {
		fwrite(bi, sizeof(*bi), 1, app_state.fh);
//		printf(" SAV\n");
	} else {
//		printf(" NOT\n");
	}

}

void layer3_app_reset(void)
{
	/* Reset state */
	app_state.has_si1 = 0;
	app_state.ccch_mode = CCCH_MODE_COMBINED;
	app_state.dch_state = DCH_NONE;
	app_state.dch_badcnt = 0;

	if (app_state.fh)
		fclose(app_state.fh);
	app_state.fh = NULL;

	memset(&app_state.cell_arfcns, 0x00, sizeof(app_state.cell_arfcns));

	downlink_cntr = 0;
	uplink_cntr = 0;
}

/* follow first Immediate Assignment */
int l3l2(struct msgb *msg, struct lapdm_entity *le, void *l3ctx)
{
	struct osmocom_ms *ms = l3ctx;
	struct abis_rsl_common_hdr *rslh;
	struct abis_rsl_rll_hdr *rllh = msgb_l2(msg);
	struct gsm48_system_information_type_header *sih;
	struct gsm48_imm_ass *ia;
	struct gsm48_hdr *gh;
	int rc;

	uint8_t ch_type, ch_subch, ch_ts, tsc;
	uint16_t arfcn;

	rslh = msgb_l2(msg);
	switch (rllh->c.msg_discr & 0xfe) {
	case ABIS_RSL_MDISC_RLL:
		switch (rllh->c.msg_type) {
		case RSL_MT_UNIT_DATA_IND:
		case RSL_MT_DATA_IND:
			rllh = msgb_l2(msg);
			rsl_dec_chan_nr(rllh->chan_nr, &ch_type, &ch_subch, &ch_ts);
			if (ch_type == RSL_CHAN_PCH_AGCH) {
//				printf("Received PCH/AGCH\n");
				sih = msgb_l3(msg);
				if (sih->system_information == GSM48_MT_RR_IMM_ASS) {
					gsm48_rx_imm_ass(msg, ms);
				}
			}
			if (ch_type == RSL_CHAN_BCCH) {
//				printf("Received BCCH\n");
			}
			if ((ch_type == RSL_CHAN_SDCCH8_ACCH) || (ch_type == RSL_CHAN_SDCCH4_ACCH)) {
//				printf("Received SDCCH\n");
				gh = msgb_l3(msg);
				switch(gh->msg_type) {
				case GSM48_MT_RR_PAG_RESP:
					gsm48_rx_pag_resp(msg, ms);
					break;
				case GSM48_MT_MM_LOC_UPD_REQUEST:
					gsm48_rx_loc_upd_req(msg, ms);
					break;
				case GSM48_MT_MM_CM_SERV_REQ:
					gsm48_rx_cm_serv_req(msg, ms);
					break;
				case GSM48_MT_MM_IMSI_DETACH_IND:
					gsm48_rx_imsi_detach(msg, ms);
					break;
				case GSM48_MT_RR_ASS_CMD:
					gsm48_rr_rx_ass_cmd(msg, ms);
					break;
				}
			}
		}

			break;
		default:
//			printf("unknown RSLms msg_discr 0x%02x\n", rllh->c.msg_discr);
			break;
	}
	msgb_free(msg);

	return 0;
}
