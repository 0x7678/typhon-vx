/*
 * Invoke gsmstack() with any kind of burst. Automaticly decode and retrieve
 * information.
 */
#include "system.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "gsmstack.h"
#include "common.h"
#include "interleave.h"
#include "cch.h"
#include "openbtsstuff/openbts.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <osmocom/core/msgb.h>
#include <osmocom/core/gsmtap.h>
#include <osmocom/core/gsmtap_util.h>

#include <l1ctl_proto.h>


extern char *uplink_amr_file;
extern char *downlink_amr_file;
extern char *filename;

static const int USEFUL_BITS = 142;


static void out_gsmdecode(char type, int arfcn, int ts, int fn, char *data, int len, unsigned char print_frames, uint8_t chan_type, int ul);

/* TODO: handle mapping in a more elegant way or simplify the function */


// Whole architecture of thi funtion is bad, move out form this file.

void init_audiofiles() {
	const unsigned char amr_nb_magic[6] = { 0x23, 0x21, 0x41, 0x4d, 0x52, 0x0a };
	int f;
	char ulfname[PATH_MAX];
	char dlfname[PATH_MAX];

// XXX dissabled until solved

// XXX mega ugly braindead hack
	// XXX change "uplink.amr. according to bursts filename"
/*	sprintf(uplink_amr_file, "%s/%s-uplink.amr", iniparser_getstring(ini, "Main:GSMSESSION", NULL),filename);
	sprintf(downlink_amr_file, "%s/%s-downlink.amr", iniparser_getstring(ini, "Main:GSMSESSION", NULL),filename);

	f=open(uplink_amr_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	write(f, amr_nb_magic, 6);
	close(f);
	f=open(downlink_amr_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	write(f, amr_nb_magic, 6);
	close(f);
*/
}



// XXX reuse in crack

uint8_t
get_chan_type(enum TIMESLOT_TYPE type, int fn, uint8_t *ss)
{
  uint8_t chan_type = GSMTAP_CHANNEL_BCCH;
  *ss = 0;
  int mf51 = fn % 51;

  if(type == TST_FCCH_SCH_BCCH_CCCH_SDCCH4)
  {
      if(mf51 == 22) /* SDCCH */
      {
          chan_type = GSMTAP_CHANNEL_SDCCH4;
          *ss = 0;
      }
      else if(mf51 == 26) /* SDCCH */
      {
          chan_type = GSMTAP_CHANNEL_SDCCH4;
          *ss = 1;
      }
      else if(mf51 == 32) /* SDCCH */
      {
          chan_type = GSMTAP_CHANNEL_SDCCH4;
          *ss = 2;
      }
      else if(mf51 == 36) /* SDCCH */
      {
          chan_type = GSMTAP_CHANNEL_SDCCH4;
          *ss = 3;
      }
      else if(mf51 == 42) /* SAACH */
      {
          chan_type = GSMTAP_CHANNEL_SDCCH4 | GSMTAP_CHANNEL_ACCH;
          *ss = ((fn % 102) > 51) ? 2 :  0;
      }
      else if(mf51 == 46) /* SAACH */
      {
          chan_type = GSMTAP_CHANNEL_SDCCH4 | GSMTAP_CHANNEL_ACCH;
          *ss = ((fn % 102) > 51) ? 3 :  1;
      }
  }
  else if(type == TST_FCCH_SCH_BCCH_CCCH)
  {
      if(mf51 != 2) /* BCCH */
      {
          chan_type = GSMTAP_CHANNEL_CCCH;
          *ss = 0;
      }
  }
  else if(type == TST_SDCCH8)
  {
      if(mf51 == 0) /* SDCCH */
      {
          chan_type = GSMTAP_CHANNEL_SDCCH8;
          *ss = 0;
      }
      else if(mf51 == 4) /* SDCCH */
      {
          chan_type = GSMTAP_CHANNEL_SDCCH8;
          *ss = 1;
      }
      else if(mf51 == 8) /* SDCCH */
      {
          chan_type = GSMTAP_CHANNEL_SDCCH8;
          *ss = 2;
      }
      else if(mf51 == 12) /* SDCCH */
      {
          chan_type = GSMTAP_CHANNEL_SDCCH8;
          *ss = 3;
      }
      else if(mf51 == 16) /* SDCCH */
      {
          chan_type = GSMTAP_CHANNEL_SDCCH8;
          *ss = 4;
      }
      else if(mf51 == 20) /* SDCCH */
      {
          chan_type = GSMTAP_CHANNEL_SDCCH8;
          *ss = 5;
      }
      else if(mf51 == 24) /* SDCCH */
      {
          chan_type = GSMTAP_CHANNEL_SDCCH8;
          *ss = 6;
      }
      else if(mf51 == 28) /* SDCCH */
      {
          chan_type = GSMTAP_CHANNEL_SDCCH8;
          *ss = 7;
      }
      else if(mf51 == 32) /* SAACH */
      {
          chan_type = GSMTAP_CHANNEL_SDCCH8 | GSMTAP_CHANNEL_ACCH;
          *ss = ((fn % 102) > 51) ? 4 :  0;
      }
      else if(mf51 == 36) /* SAACH */
      {
          chan_type = GSMTAP_CHANNEL_SDCCH8 | GSMTAP_CHANNEL_ACCH;
          *ss = ((fn % 102) > 51) ? 5 :  1;
      }
      else if(mf51 == 40) /* SAACH */
      {
          chan_type = GSMTAP_CHANNEL_SDCCH8 | GSMTAP_CHANNEL_ACCH;
          *ss = ((fn % 102) > 51) ? 6 :  2;
      }
      else if(mf51 == 44) /* SAACH */
      {
          chan_type = GSMTAP_CHANNEL_SDCCH8 | GSMTAP_CHANNEL_ACCH;
          *ss = ((fn % 102) > 51) ? 7 :  3;
      }
  }
  else if (type == TST_TCHF) {
    chan_type = GSMTAP_CHANNEL_TCH_F | GSMTAP_CHANNEL_ACCH;
  }

  return chan_type;
}

/*
 * Initialize a new GSMSTACK context.
 */
int
GS_new(GS_CTX *ctx)
{
	int rc;
	struct sockaddr_in sin;

	sin.sin_family = AF_INET;
	sin.sin_port = htons(GSMTAP_UDP_PORT);
	inet_aton("127.0.0.1", &sin.sin_addr);

	memset(ctx, 0, sizeof *ctx);
	interleave_init(&ctx->interleave_ctx, 456, 114);
	interleave_init_facch_f(&ctx->interleave_facch_f1_ctx, 456, 114, 0);
	interleave_init_facch_f(&ctx->interleave_facch_f2_ctx, 456, 114, 4);
	ctx->fn = -1;
	ctx->bsic = -1;
	ctx->gsmtap_fd = -1;

	rc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (rc < 0) {
		perror("creating UDP socket\n");
		return rc;
	}
	ctx->gsmtap_fd = rc;
	rc = connect(rc, (struct sockaddr *)&sin, sizeof(sin));
	if (rc < 0) {
		perror("connectiong UDP socket");
		close(ctx->gsmtap_fd);
		return rc;
	}

	return 0;
}

#define BURST_BYTES	((USEFUL_BITS/8)+1)
/*
 * 142 bit
 */

/* return values
 * -1 decoding failed
 * 0  not enough burts to decode yet
 * 1  successfully decoded
 * 2  successfully decoded SI5, SI6
 */
int
GS_process(GS_CTX *ctx, struct l1ctl_burst_ind *bi, unsigned char d_KC[8], int first_burst, unsigned char print_frames)
{
	int bsic;
	int ret;
	int ts = bi->chan_nr & 7;
	unsigned char *data;
	int len;
	struct gs_ts_ctx *ts_ctx = &ctx->ts_ctx[ts];
	unsigned char burst_binary[58+26+58];
	unsigned char src[58+26+58];
	int i;
	int fn, ul;
	int type = NORMAL;
	struct gsm_time tm;

	ul = !!(ntohs(bi->band_arfcn) & ARFCN_UPLINK);
	gsm_fn2gsmtime(&tm, ntohl(bi->frame_nr));
	fn = tm.fn;



	/* INPUT 57+57+1+1  OUTPUT 57+1+26+1+57 */
        for(i=0; i<57; i++)
		burst_binary[i] = !!(bi->bits[i / 8] & (1 << (7 - (i % 8))));
	burst_binary[57] = !!(bi->bits[114 / 8] & (1 << (7 - (114 % 8))));
	for(i=58; i<58+26; i++)
		burst_binary[i] = 2;
	burst_binary[58+26] = !!(bi->bits[115 / 8] & (1 << (7 - (115 % 8))));
	for(i=59; i<116; i++)
		burst_binary[i + 26] = !!(bi->bits[(i-2) / 8] & (1 << (7 - ((i-2) % 8))));

	decrypt(burst_binary, d_KC, src, (tm.t1<<11)|(tm.t3<<5)|tm.t2, ul);

	memset(ctx->msg, 0, sizeof(ctx->msg));


	if (ts_ctx->type == TST_TCHF && type == NORMAL &&
	    (fn % 26) != 12 && (fn % 26) != 25) {


//		if (!(uplink_amr_file && downlink_amr_file )) {
//			init_audiofiles();
//		}
		if (tch_process_burst(src, ts, fn, ul)) {
//			int f;
//			if (ul)
//				f=open(uplink_amr_file, O_WRONLY | O_APPEND);
//			else
//				f=open(downlink_amr_file, O_WRONLY | O_APPEND);
//			write(f,get_voice_frame(ul), get_voice_frame_length(ul));
//			close(f);
		}
	
	}

	if (ts_ctx->type == TST_TCHF && type == NORMAL &&
	    (fn % 26) != 12 && (fn % 26) != 25) {
		/* Dieter: we came here because the burst might contain FACCH bits */
		ctx->fn = fn;

		/* get burst index to TCH bursts only */
		ts_ctx->burst_count2 = fn % 26;

		if (ts_ctx->burst_count2 >= 12)
			ts_ctx->burst_count2--;
		ts_ctx->burst_count2 = ts_ctx->burst_count2 % 8;

		/* copy data bits and stealing flags to buffer */
		memcpy(ts_ctx->burst2 + (116 * ts_ctx->burst_count2), src, 58);
		memcpy(ts_ctx->burst2 + (116 * ts_ctx->burst_count2) + 58, src + 58 + 26, 58);

		/* Return if not enough bursts for a full gsm message */
		if ((ts_ctx->burst_count2 % 4) != 3)
			return 0;

		data = decode_facch(ctx, ts_ctx->burst2, &len, (ts_ctx->burst_count2 == 3) ? 1 : 0);
		if (data == NULL) {
//			printf("cannot decode FACCH fnr=%d ts=%d\n", ctx->fn, ts);
			return -1;
		}

		out_gsmdecode(0, 0, ts, ctx->fn, data, len, print_frames, GSMTAP_CHANNEL_TCH_F, ul);

		if (ctx->gsmtap_fd >= 0) {
			struct msgb *msg;
			uint8_t chan_type = GSMTAP_CHANNEL_TCH_F;
			uint8_t ss = 0;
			int fn = (ctx->fn - 3); /*  "- 3" for start of frame */

			msg = gsmtap_makemsg(ntohs(bi->band_arfcn), ts, chan_type, ss, ctx->fn, 0, 0, data, len);
			if (msg)
				write(ctx->gsmtap_fd, msg->data, msg->len);
		}
		return 1;
	}


	if (ts_ctx->type == TST_TCHF) {
		if (tm.t2 == 12 || tm.t2 == 25) { /* SACCH of Full Rate TCH */
			if (ts % 2 == 0) /* SACH position and start depends on the timeslot */
				first_burst = (fn % 104) == (12 + 26 * (ts / 2));
			else
				first_burst = (fn % 104) == (25 + 26 * ((ts - 1) / 2));
		}
	}
	if (ts_ctx->type == TST_TCHF && type == NORMAL &&
	    (fn % 26) != 12 && (fn % 26) != 25) {
		return 0;
	}

	/* normal burst processing */
	if (first_burst) /* Dieter: it is important to start with the correct burst */
		ts_ctx->burst_count = 0;

	ctx->fn = fn;
	if (type == NORMAL) {
		/* Interested in these frame numbers (cch)
 		 * 2-5, 12-15, 22-25, 23-35, 42-45
 		 * 6-9, 16-19, 26-29, 36-39, 46-49
 		 */
		/* Copy content data into new array */
		//DEBUGF("burst count %d\n", ctx->burst_count);
		memcpy(ts_ctx->burst + (116 * ts_ctx->burst_count), src, 58);
		memcpy(ts_ctx->burst + (116 * ts_ctx->burst_count) + 58, src + 58 + 26, 58);
		ts_ctx->burst_count++;
		/* Return if not enough bursts for a full gsm message */
		if (ts_ctx->burst_count < 4)
			return 0;

		ts_ctx->burst_count = 0;
		data = decode_cch(ctx, ts_ctx->burst, &len);
		if (data == NULL) {
//			printf("cannot decode fnr=0x%06x (%6d) ts=%d\n", ctx->fn, ctx->fn, ts);
			return -1;
		}
		//DEBUGF("OK TS %d, len %d\n", ts, len);

		/* If we are here and key is set, key is OK */
		if (d_KC[0] | d_KC[1] | d_KC[2] | d_KC[3] | d_KC[4] | d_KC[5] | d_KC[6] | d_KC[7] )
			printf("KEY_OK\n");

		memcpy(ctx->msg, data, 23);

		if (ctx->gsmtap_fd >= 0) {
			/* Dieter: set channel type according to configuration */
			struct msgb *msg;
			uint8_t chan_type;
			uint8_t ss = 0;
			int fn = (ctx->fn - 3); /*  "- 3" for start of frame */

			chan_type = get_chan_type(ts_ctx->type, fn-(ul * 15), &ss);
//	printf("DBG: Channel_type: %x\n",chan_type);

			/* arfcn, ts, chan_type, ss, fn, signal, snr, data, len */
			msg = gsmtap_makemsg(ntohs(bi->band_arfcn), ts, chan_type, ss,
					     ctx->fn, 0, 0, data, len);

			out_gsmdecode(0, 0, ts, ctx->fn, data, len, print_frames, chan_type, ul);

			if (msg)
				write(ctx->gsmtap_fd, msg->data,
				      msg->len);
		}
		unsigned char SI5[] = {0x03, 0x03, 0x49, 0x06, 0x1d};
		unsigned char SI5TER[] = {0x03, 0x03, 0x49, 0x06, 0x06};
		unsigned char SI6[] = {0x03, 0x03, 0x2d, 0x06, 0x1e};
		if (!strncmp(data+2, SI5, 5))
			return 2;
		if (!strncmp(data+2, SI5TER, 5))
			return 2;
		if (!strncmp(data+2, SI6, 5))
			return 2;

		return 1;
	}
}


/*
 * Output data so that it can be parsed from gsmdecode.
 */
static void
out_gsmdecode(char type, int arfcn, int ts, int fn, char *data, int len, unsigned char print_frames, uint8_t chan_type, int ul)
{
	char *end = data + len;

	if(print_frames) {
		printf("FRAME: %6d %d", (fn + 0), ts);
		if (chan_type & GSMTAP_CHANNEL_ACCH) {
			printf(" SACCH/", GSMTAP_CHANNEL_SDCCH4, GSMTAP_CHANNEL_SDCCH8, chan_type, GSMTAP_CHANNEL_ACCH);
		} else {
			printf(" SDCCH/");
		}
		if ((chan_type & (~GSMTAP_CHANNEL_ACCH)) == GSMTAP_CHANNEL_SDCCH4) {
			printf("4 ");
		}
		if ((chan_type & (~GSMTAP_CHANNEL_ACCH)) == GSMTAP_CHANNEL_SDCCH8) {
			printf("8 ");
		}
		if(ul) {
			printf("UL: ");
		} else {
			printf("DL: ");
		}

	
		/* FIXME: speed this up by first printing into an array */
		while (data < end)
			printf("%02.2x", (unsigned char)*data++);
		printf("\n");
		fflush(stdout);
	}
}
