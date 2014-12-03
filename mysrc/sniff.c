/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <osmocom/bb/common/osmocom_data.h>
#include <osmocom/bb/common/l1ctl.h>
#include <osmocom/bb/common/logging.h>
#include <osmocom/bb/common/l23_app.h>
#include <osmocom/bb/misc/layer3.h>

#include <osmocom/core/msgb.h>
#include <osmocom/core/talloc.h>
#include <osmocom/core/select.h>
#include <osmocom/gsm/protocol/gsm_08_58.h>

#include <time.h>

#include "gsmstack.h"
#include "custom_ms.h"
#include "common.h"
#include <fcntl.h>


static int t;
static int chan = 0x90;
static int tsc = 0;
static int first = 1;

extern assignment transport;
extern app_state_t app_state;

GS_CTX d_gs_ctx_downlink;
GS_CTX d_gs_ctx_downlink_crypt;
GS_CTX d_gs_ctx_uplink;
GS_CTX d_gs_ctx_uplink_crypt;
unsigned char d_KC[8] = {0, 0, 0, 0, 0, 0, 0, 0};
unsigned char mytmsi[4] = {0, 0, 0, 0};
char *uplink_amr_file = NULL;
char *downlink_amr_file = NULL;

// operator & bts identification for current burst
// first 3 digits: country code , last 2 digits: operator code
extern uint32_t b_mcnc;
extern uint32_t b_cellid;

int sync_errs = 0;
#define MAX_SYNC_ERRORS 10

int mobile_delete(struct osmocom_ms *ms, int force) {};
int mobile_init(struct osmocom_ms *ms) {};
int mobile_exit(struct osmocom_ms *ms, int force) {};
struct osmocom_ms *mobile_new(char *name) {};
int sap_open(struct osmocom_ms *ms, const char *socket_path){};
const struct log_info log_info;

int l3l2(struct msgb *msg, struct lapdm_entity *le, void *l3ctx);

// Komentare!

static int signal_cb(unsigned int subsys, unsigned int signal,
                     void *handler_data, void *signal_data)
{
	int rc;
        struct osmocom_ms *ms;
	struct osmobb_fbsb_res *fr;
	struct osmobb_msg_ind *mi;

	//assignment transport;

        if (subsys != SS_L1CTL)
                return 0;

//	printf("signal: %d, first: %d\n",signal,first); // DBG



        switch (signal) {

        case S_L1CTL_RESET:
		ms = signal_data;
//		printf("Received signal S_L1CTL_RESET\n");
		if (!first) {
			return(0);
		}
		// slave loop
		if(mode == MODE_SLAVE) {
			printf("Slave RESET\n");	
			release(ms);
		}
		rc = l1ctl_tx_fbsb_req(ms, ms->test_arfcn, L1CTL_FBSB_F_FB01SB, 100, 0, CCCH_MODE_NON_COMBINED);
		break;

	case S_L1CTL_FBSB_ERR:
		fr = signal_data;
		ms = fr -> ms;
		printf("Received signal S_L1CTL_FBSB_ERR:\n");
		sync_errs++;
		if(sync_errs>MAX_SYNC_ERRORS) {
			exit(42+42);
		}
		l1ctl_tx_reset_req(ms, L1CTL_RES_T_FULL);
		break;

	case S_L1CTL_FBSB_RESP:
		fr = signal_data;
		ms = fr -> ms;
//		printf("Received signal S_L1CTL_FBSB_RESP\n");
		alarm(0);
		sync_errs=0;
		if(mode == MODE_SLAVE) {
//			printf("DBG: Slave_RESP %d\n",transport.ass_type);

			assert(transport.kanarek == 0xdefeca7e);

			// if tmsi is empty, save time on lookup
			if ( transport.tmsi != 0 )  {
				get_key(transport.tmsi);
			}

			//0 - nonhopping imm_ass
			if (transport.ass_type == 0) {
				printf("JUMP: Slave, 0, chan_nr: %x, arfcn: %d, tsc: %d\n",transport.chan_nr, transport.arfcn, transport.tsc);
				l1ctl_tx_dm_est_req_h0(ms,
        		               	transport.arfcn, transport.chan_nr, transport.tsc,
                		       	GSM48_CMODE_SIGN, 0); // XXX  GSM48_CMODE_SPEECH_AMR
			}

			//1 - hopping imm_ass
			if (transport.ass_type == 1) {
				printf("JUMP: Slave, 1, chan_nr: %x, tsc: %d\n", transport.chan_nr, transport.tsc);
				l1ctl_tx_dm_est_req_h1(ms,
        		               	transport.maio, transport.hsn, transport.ma,
					transport.ma_len,transport.chan_nr,transport.tsc,
                		       	GSM48_CMODE_SIGN, 0); // XXX  GSM48_CMODE_SPEECH_AMR
			}
		
			/* Set state */
			app_state.dch_state = DCH_WAIT_EST;
			app_state.dch_nr = transport.chan_nr;
			b_mcnc=transport.b_mcnc;
			b_cellid=transport.b_cellid;
			app_state.dch_badcnt = 0;

		} else {
			layer3_app_reset();
		}
                break;

	case S_L1CTL_BURST_IND:
//		printf("Received signal L1CTL_BURST_IND\n");
		mi = signal_data;
		layer3_rx_burst(mi->ms, mi->msg);
		break;
        }
        return 0;
}

int sniff_work(struct osmocom_ms *ms)
{
	struct msgb *msg;
	struct gsm48_rrlayer *rr = &ms->rrlayer;
	struct abis_rsl_common_hdr *rslh;

	while ((msg = msgb_dequeue(&rr->rsl_upqueue))) {
		rslh = msgb_l2(msg);
		printf("DISCR=%x\n", rslh->msg_discr & 0xfe);
		l3l2(msg, NULL, ms);
	}
	msgb_free(msg);
}

void set_key(char *key) {
	read_key(key, d_KC);
}

void set_tmsi(char *tmsi) {
	printf("TMSI: '%s'\n", tmsi);
	sscanf(tmsi, "%2hhx%2hhx%2hhx%2hhx", mytmsi,  mytmsi+1, mytmsi+2, mytmsi+3);
	printf ("%2x %2x %2x %2x", mytmsi[0],  mytmsi[1], mytmsi[2], mytmsi[3]);
}

int l23_app_init(struct osmocom_ms *ms)
{
	int rc;
printf("l23init, master:%i\n", mode);
	parse_ini_file();

	gsm48_rr_init(ms);

	init_custom_ms(ms);

	osmo_signal_register_handler(SS_L1CTL, &signal_cb, NULL);
	l1ctl_tx_reset_req(ms, L1CTL_RES_T_FULL);
	l1ctl_tx_dm_rel_req(ms);

	l23_app_work = sniff_work;
	lapdm_channel_set_l3(&ms->lapdm_channel, &l3l2, ms);

	tch_init();

	GS_new(&d_gs_ctx_uplink_crypt);
	GS_new(&d_gs_ctx_uplink);
	GS_new(&d_gs_ctx_downlink);
	GS_new(&d_gs_ctx_downlink_crypt);
	d_gs_ctx_downlink.gsmtap_fd=-1;
	d_gs_ctx_downlink_crypt.gsmtap_fd=-1;
        d_gs_ctx_uplink_crypt.ts_ctx[0].type = TST_FCCH_SCH_BCCH_CCCH_SDCCH4;
        d_gs_ctx_uplink.ts_ctx[0].type = TST_FCCH_SCH_BCCH_CCCH_SDCCH4;
        d_gs_ctx_downlink_crypt.ts_ctx[0].type = TST_FCCH_SCH_BCCH_CCCH_SDCCH4;
        d_gs_ctx_downlink.ts_ctx[0].type = TST_FCCH_SCH_BCCH_CCCH_SDCCH4;
        d_gs_ctx_uplink.ts_ctx[1].type = TST_SDCCH8;		/* TST_TCHF TST_SDCCH8 TST_FCCH_SCH_BCCH_CCCH TST_FCCH_SCH_BCCH_CCCH */
        d_gs_ctx_downlink.ts_ctx[1].type = TST_SDCCH8;		/* TST_TCHF TST_SDCCH8 TST_FCCH_SCH_BCCH_CCCH TST_FCCH_SCH_BCCH_CCCH */


	return 0;

}

