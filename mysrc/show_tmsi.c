/* TEST code, regularly transmit ECHO REQ packet to L1 */

/* (C) 2010 by Holger Hans Peter Freyther
 * (C) 2010 by Harald Welte <laforge@gnumonks.org>
 *
 * All Rights Reserved
 *
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

#include <l1ctl_proto.h>

#include <time.h>


static int t;
static int chan = 0x90;
static int tsc = 0;
static int first = 1;

int mobile_delete(struct osmocom_ms *ms, int force) {};
int mobile_init(struct osmocom_ms *ms) {};
int mobile_exit(struct osmocom_ms *ms, int force) {};
struct osmocom_ms *mobile_new(char *name) {};
int sap_open(struct osmocom_ms *ms, const char *socket_path){};
void set_key(char *key) {};
void set_tmsi(char *tmsi) {};
const struct log_info log_info;



static int signal_cb(unsigned int subsys, unsigned int signal,
                     void *handler_data, void *signal_data)
{
	int rc;
        struct osmocom_ms *ms;
	struct osmobb_fbsb_res *fr;


        if (subsys != SS_L1CTL)
                return 0;



        switch (signal) {

        case S_L1CTL_RESET:
		ms = signal_data;
		printf("XXX Received signal S_L1CTL_RESET\n");
		if (!first)
			return(0);
	        rc = l1ctl_tx_fbsb_req(ms, ms->test_arfcn, L1CTL_FBSB_F_FB01SB, 100, 0, CCCH_MODE_COMBINED);
		break;

	case S_L1CTL_FBSB_ERR:
		ms = signal_data;
		printf("XXX Received signal S_L1CTL_FBSB_ERR:\n");
	        rc = l1ctl_tx_fbsb_req(ms, ms->test_arfcn, L1CTL_FBSB_F_FB01SB, 100, 0, CCCH_MODE_NON_COMBINED);
		l1ctl_tx_reset_req(ms, L1CTL_RES_T_FULL);
		break;

	case S_L1CTL_FBSB_RESP:
		fr = signal_data;
		ms = fr -> ms;
		printf("XXX Received signal S_L1CTL_FBSB_RESP\n");
//		rc = l1ctl_tx_dm_est_req_h0(ms,ms->test_arfcn,0x41,2,GSM48_CMODE_SIGN);
                break;
        }
        return 0;
}

// XXX this f*cking function actually SET tmsi, instead of printig it  XXX fix

void print_mi(uint8_t *mi, int num)
{
	uint8_t mi_type;
	uint32_t tmsi;
	char imsi[16];

	mi_type = mi[1] & GSM_MI_TYPE_MASK;
	switch (mi_type) {
		case GSM_MI_TYPE_TMSI:
			memcpy(&tmsi, mi+2, 4);
			printf("TMSI-%i %08x\n", num, ntohl(tmsi));
			break;
		case GSM_MI_TYPE_IMSI:
			gsm48_mi_to_string(imsi, sizeof(imsi), mi + 1, mi[0]);
			printf("IMSI-%i %s\n", num, imsi);
			break;
		case GSM_MI_TYPE_NONE:
			break;
		default:
			printf("Unknown MI type\n");
	}
}

int pokus_work(struct osmocom_ms *ms)
{

	struct gsm48_rrlayer *rr = &ms->rrlayer;
	struct msgb *msg;
	struct abis_rsl_common_hdr *rslh;
	struct abis_rsl_rll_hdr *rllh;
	struct gsm48_paging1 *pa;
	int rc;
	uint8_t *mi;
	int payload_len;

			struct gsm48_system_information_type_header *sih;
			struct gsm48_imm_ass *ia;
			struct gsm48_rr_cd *cd = &rr->cd_now;
			uint8_t ch_type, ch_subch, ch_ts, tsc;
			uint16_t arfcn;

	while ((msg = msgb_dequeue(&rr->rsl_upqueue))) {
		rslh = msgb_l2(msg);
		rllh = msgb_l2(msg);
		sih = msgb_l3(msg);
		ia = msgb_l3(msg);
		if (((rslh->msg_discr & 0xfe) == ABIS_RSL_MDISC_RLL) && (sih->system_information == GSM48_MT_RR_PAG_REQ_1)) {
			pa = msgb_l3(msg);
			mi = pa->data;
			payload_len = msgb_l3len(msg) - sizeof(*pa);
			print_mi(mi,1);
			payload_len -= mi[0] + 1;
			mi = pa->data + mi[0] + 1;
			if ((payload_len > 1) && (mi[0] == GSM48_IE_MOBILE_ID))

				print_mi(mi+1,2);
		}
				
	msgb_free(msg);

        }



	return 0;
}

int l23_app_init(struct osmocom_ms *ms)
{
	int rc;

	gsm48_rr_init(ms);

	osmo_signal_register_handler(SS_L1CTL, &signal_cb, NULL);
	l1ctl_tx_reset_req(ms, L1CTL_RES_T_FULL);

	l23_app_work = pokus_work;

	return 0;
}
