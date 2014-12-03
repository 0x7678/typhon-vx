#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <sqlite3.h>

#include <osmocom/bb/common/osmocom_data.h>
#include <l1ctl_proto.h>

#include <osmocom/core/gsmtap_util.h>
#include <osmocom/core/gsmtap.h>

#include "gsmstack.h"
#include "common.h"
#include "openbtsstuff/openbts.h"


char filename[PATH_MAX];
unsigned char d_KC[8] = {0, 0, 0, 0, 0, 0, 0, 0};
int timeslot_type = TST_FCCH_SCH_BCCH_CCCH;
int channel_type=0;
int tmp=0,ACCH_length_dl=0, DCCH_length_dl=0, ACCH_length_ul=0, DCCH_length_ul=0;
int use_statistics=0;
float stats_threshold=0; // do not cut anything by default
char targettype = 'd'; // default for NOP
char uplink_amr_file[PATH_MAX], downlink_amr_file[PATH_MAX];

sqlite3 *conn;
sqlite3 *aconn;          // optional DB to merge
unsigned char use_adb=0; // use it?
sqlite3_stmt    *sqlres;
int     sqlerr = 0;
int     rec_count = 0;
const char      *errMSG;
const char      *tail;
char query[PATH_MAX];

// WTF??? 114 bytes instead of 114 bits? WTF?
unsigned char ACCH_array_dl[MAX_BURSTS_PER_FILE][BURST_SIZE];
unsigned char DCCH_array_dl[MAX_BURSTS_PER_FILE][BURST_SIZE];
unsigned char ACCH_array_ul[MAX_BURSTS_PER_FILE][BURST_SIZE];
unsigned char DCCH_array_ul[MAX_BURSTS_PER_FILE][BURST_SIZE];

unsigned int ACCH_array_help_dl[MAX_BURSTS_PER_FILE][2];
unsigned int DCCH_array_help_dl[MAX_BURSTS_PER_FILE][2];
unsigned int ACCH_array_help_ul[MAX_BURSTS_PER_FILE][2];
unsigned int DCCH_array_help_ul[MAX_BURSTS_PER_FILE][2];

#define CHANTYPE_SDCCH_DL_SORTED 0
#define CHANTYPE_SACCH_DL 1
#define CHANTYPE_SDCCH_UL_SORTED 2
#define CHANTYPE_SACCH_UL 3
#define CHANTYPE_SDCCH_DL 4
#define CHANTYPE_SDCCH_UL 5

unsigned char guess_chantypes[] = {'0', '1', '5', '3'};

// default downlink NOP with padding
unsigned char tstframe[] = {0x03, 0x03, 0x01, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,
                            0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,
                            0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b};
unsigned char uplink = 0;
unsigned char noattack = 0;
int snr_cut = 50;

unsigned char guess_sdcch = 0;
unsigned char guess_sacch = 0;

void getplaintext(const char* param, unsigned char *tmpframe) {
	int count;
	for(count = 0; count < FRAME_SIZE; count++) {
		sscanf(param, "%2hhx", &tmpframe[count]);
		param += 2 * sizeof(char);
	}
}

void print_help()
{
	printf("Usage: gsm_convert -f filename [-k KEY]\n");
        printf(" Some help...\n");
        printf("  -h, --help            this text\n");
        printf("  -f, --file DATFILE    bursts file\n");
        printf("  -k, --key KEY         Kc\n");
        printf("  -4, -sdcch4           set channel mode to SDCCH4\n");
        printf("  -8, -sdcch8           set channel mode to SDCCH8 (default)\n");
	printf("  -p                    provide known plaintext (default is downlink func UI with 0x2b padding)\n");
	printf("  -d                    dry run - do not attempt known-plaintext attack\n");
	printf("  -l, TYPE              type of logical channels to guess keystream for, d for SDCCH (default), a for SACCH\n");
        printf("  -u                    guess keystream for uplink\n");
	printf("  -s, --statistical db  automatically get most probable plaintext for each frame from database file.\n");
	printf("  -t, --threshold prob. use only plaintexts with probability at least prob. (used only with -s)\n");
	printf("                        (Also suppress verbose messages. Other options are mostly ignored in this mode.)\n");
	printf("  -n                    cutoff SNR level (default 10)\n");
}

static void handle_options(int argc, char **argv)
{

	char *myfile=NULL;
	int i=0,noguess=0;
        while (1) {
                int option_index = 0, c;
                static struct option long_options[] = {
                        {"help", 0, 0, 'h'},
                        {"file", 1, 0, 'f'},
                        {"key", 1, 0, 'k'},
                        {"sdcch4", 0, 0, '4'},
                        {"sdcch8", 0, 0, '8'},
                        {"statstical", 1, 0, 's'},
                        {"threshold", 1, 0, 't'},
                        {0, 0, 0, 0},
                };

                c = getopt_long(argc, argv, "h48f:k:p:l:udn:s:t:g:a:b:",
                                long_options, &option_index);
                if (c == -1)
                        break;

                switch (c) {
                case 'h':
                        print_help();
                        exit(0);
                        break;
                case 'f':
                        strncpy(filename, optarg, PATH_MAX);
                        break;
                case 'k':
			read_key(optarg, d_KC);
                        break;
                case '4':
			timeslot_type=TST_FCCH_SCH_BCCH_CCCH_SDCCH4;
			noguess=1;
                        break;
                case '8':
			timeslot_type=TST_FCCH_SCH_BCCH_CCCH;
			noguess=1;
                        break;
		case 'd':
			noattack=1;
			break;
                case 'u':
			uplink=1;
                        break;
		case 'p':
			if(strlen(optarg) == 81*2) {
				printf("Received the whole GSMTAP frame as a source of known plaintext\n.");
				optarg+=116;
			} else if (strlen(optarg) == 23*2) {
				printf("Got known plaintext\n");
			} else {
				printf("23 bytes of known plaintext must be supplied (got %d nibbles)\n", strlen(optarg));
				exit(42);
			}
			getplaintext(optarg,tstframe);
			break;
		case 'l':
			if(optarg[0] != 'd' && optarg[0] != 'a') {
				printf("Specify either d (SDCCH) or a (SACCH) logical channel\n\n");
				exit(42);
			}
			targettype = optarg[0];
			if(strchr(optarg, 'd')) { // crap ;)
				guess_sdcch = 1;
			}
			if(strchr(optarg, 'a')) {
				guess_sacch = 1;
			}
			break;
		case 'n':
			snr_cut=atoi(optarg);
			break;
		case 's':
			use_statistics=1;

			sqlerr = sqlite3_open(optarg, &conn);
			if (sqlerr) {
				printf("Can not open database err = %i\n", sqlerr);
				exit(42);
			}

			break;
		case 'a':
			sqlerr = sqlite3_open(optarg, &aconn);
			if (sqlerr) {
				printf("Can not open cell-specific database err = %i\n", sqlerr);
				exit(42);
			}
			use_adb = 1;
			break;
		case 't':
			stats_threshold=atof(optarg);
//			fprintf(stderr,"Mrdat krecky: %f\n",stats_threshold);
			// XXX check error
			break;
		case 'g':
			strncpy(guess_chantypes,optarg,4);
			break;
		default:
			print_help();
			exit(0);
			break;
		}
	}

	if (noguess==0) {


		// guess sdcch4/8 from filename
		// hex number after 5th '_' should contain channel number
		// if < 0x40, its is sdcch4, otherwise sdcch 8
		// see "bi->chan_nr" in l3.c
	
		myfile=(char *)basename(filename);
		if (strlen(myfile) <5) {
			print_help();
			exit(45);
		}

		for (i=0;i<5;i++) {
			myfile=strchr(myfile,'_');
			if (myfile[0] == '_') { myfile++;}
		}
		if (myfile == NULL) {
			printf("Incorrect filename format for guess.\n");
			timeslot_type=TST_FCCH_SCH_BCCH_CCCH_SDCCH4;
		} else {
			if (myfile[0] >= '4') {
		//		fprintf(stderr,"Guessed sdcch8\n");
				timeslot_type=TST_FCCH_SCH_BCCH_CCCH;
			} else {
		//		fprintf(stderr,"Guessed: sdcch4\n");
				timeslot_type=TST_FCCH_SCH_BCCH_CCCH_SDCCH4;
			}
		}
	}

}

void guesser(int chantype, sqlite3 *myconn) {

	unsigned char tmp_frame[FRAME_SIZE]; // one frame (binary without redundancy)
	unsigned char tmp_array[4][BURST_SIZE]; // one frame (encoded 1 bit in one byte)
	unsigned char tmp_used_dcch_bursts[MAX_BURSTS_PER_FILE]; // to mark if we already used this ciphertext to produce keystream
	int k,x=0;
	int ACCH_length = ACCH_length_dl;
	int DCCH_length = DCCH_length_dl;

	unsigned char sorted_guesser = 0;
	unsigned char use_sacch_plugin_now = 0;
/*	if(chantype == 0 || chantype == 2) { // sorted guesser for SDCCH - take care of len and positions in transaction
		sorted_guesser = 1;
	}*/
	unsigned char UL = 0;
	if(chantype == CHANTYPE_SDCCH_UL_SORTED || chantype == CHANTYPE_SACCH_UL || chantype == CHANTYPE_SDCCH_UL) {
		UL = 1;
		DCCH_length = DCCH_length_ul;
		ACCH_length = ACCH_length_ul;
	}
	if(chantype == CHANTYPE_SDCCH_DL_SORTED || chantype == CHANTYPE_SDCCH_UL_SORTED) {
/*		if(UL) {
			DCCH_length = DCCH_length_ul;
		}*/
		sprintf(query, "SELECT x.position,y.plain,x.probability FROM stats x join plaintexts y where "\
			"x.length=%i and x.id_plain=y.id and x.type=%i and x.probability > %f ORDER BY x.probability desc",DCCH_length/4,chantype,stats_threshold);
	} else { // unsorted guesser
/*		if(chantype == CHANTYPE_SACCH_UL) { 
			ACCH_length = ACCH_length_ul;
		}*/
		sprintf(query, "SELECT y.plain,x.probability FROM stats x join plaintexts y where "\
			"x.type=%i and x.id_plain=y.id and x.probability > %f ORDER BY x.probability desc;",chantype,stats_threshold);
	}
	sqlerr = SQLITE_OK;
	do { // DB can be locked sometimes (other process changing statistics)
        	sqlerr = sqlite3_prepare_v2(myconn, query, MAX_SQL_QUERY, &sqlres, &tail); 
		usleep(SQL_RETRY_TIME);
		if(sqlerr != SQLITE_OK) {
			fprintf(stderr,"stats select err: %s\n",sqlite3_errmsg(myconn));
		}
	} while (sqlerr != SQLITE_OK);

	/* Data received, now process them */
	if(chantype == CHANTYPE_SDCCH_DL_SORTED || chantype == CHANTYPE_SDCCH_UL_SORTED) {
	        while (SQLITE_ROW == sqlite3_step(sqlres)) {
			getplaintext(sqlite3_column_text(sqlres,1),tmp_frame);
			packet_encode(tmp_frame,(unsigned char *)&tmp_array);

			for (x=0;x<4;x++) {
				int index=(((sqlite3_column_int(sqlres,0)-1)*4)+x);
				if(chantype == CHANTYPE_SDCCH_UL_SORTED) {
					printf("%5f SDCCH/UL %u %u: ",sqlite3_column_double(sqlres,2),DCCH_array_help_ul[index][0],DCCH_array_help_ul[index][1] );
					for(k=0;k<BURST_SIZE;k++) {
						printf("%i",DCCH_array_ul[index][k] ^ tmp_array[x][k]);
					}
				} else {
					printf("%5f SDCCH/DL %u %u: ",sqlite3_column_double(sqlres,2),DCCH_array_help_dl[index][0],DCCH_array_help_dl[index][1] );
					for(k=0;k<BURST_SIZE;k++) {
						printf("%i",DCCH_array_dl[index][k] ^ tmp_array[x][k]);
					}
				}
				printf("\n");
			}
			printf("\n");
		}
	} else { // unsorted guesser - may be all 4 types
	        while (SQLITE_ROW == sqlite3_step(sqlres)) {
			getplaintext(sqlite3_column_text(sqlres,0),tmp_frame);
			packet_encode(tmp_frame,(unsigned char *)&tmp_array);

			int arr_pos;
			if(chantype == CHANTYPE_SACCH_DL || chantype == CHANTYPE_SACCH_UL) {
				for (arr_pos=0; arr_pos<ACCH_length/4; arr_pos++) { // iterate over the whole array of captured data
					for (x=0;x<4;x++) {
						int index=(((arr_pos)*4)+x);
						if(chantype == CHANTYPE_SACCH_UL) {
							printf("%5f SACCH/UL %u %u: ",sqlite3_column_double(sqlres,1),ACCH_array_help_ul[index][0],ACCH_array_help_ul[index][1] );
							for(k=0;k<BURST_SIZE;k++) {
								printf("%i",ACCH_array_ul[index][k] ^ tmp_array[x][k]);
							}
						} else {
							printf("%5f SACCH/DL %u %u: ",sqlite3_column_double(sqlres,1),ACCH_array_help_dl[index][0],ACCH_array_help_dl[index][1] );
							for(k=0;k<BURST_SIZE;k++) {
								printf("%i",ACCH_array_dl[index][k] ^ tmp_array[x][k]);
							}
						}
						printf("\n");
					}
					printf("\n");
				}
			} else { // CHANTYPE_SDCCH_DL, CHANTYPE_SDCCH_UL
				for (arr_pos=0; arr_pos<DCCH_length/4; arr_pos++) { // iterate over the whole array of captured data
					for (x=0;x<4;x++) {
						int index=(((arr_pos)*4)+x);
						if(chantype == CHANTYPE_SDCCH_UL) {
							printf("%5f SDCCH/UL %u %u: ",sqlite3_column_double(sqlres,1),DCCH_array_help_ul[index][0],DCCH_array_help_ul[index][1] );
							for(k=0;k<BURST_SIZE;k++) {
								printf("%i",DCCH_array_ul[index][k] ^ tmp_array[x][k]);
							}
						} else {
							printf("%5f SDCCH/DL %u %u: ",sqlite3_column_double(sqlres,1),DCCH_array_help_dl[index][0],DCCH_array_help_dl[index][1] );
							for(k=0;k<BURST_SIZE;k++) {
								printf("%i",DCCH_array_dl[index][k] ^ tmp_array[x][k]);
							}
						}
						printf("\n");
					}
					printf("\n");
				}
			}
		}
	}

}

void main(int argc, char **argv) {

	int i, j, k, f, ul, res;
	struct l1ctl_burst_ind *bi;
	GS_CTX d_gs_ctx_downlink;
	GS_CTX d_gs_ctx_uplink;
	int downlink_cntr=0;
	int uplink_cntr=0;
	struct gsm_time tm;
	unsigned char known_plaintext[4][BURST_SIZE];
	unsigned char bursts[4][BURST_SIZE];

	handle_options(argc, argv);
	packet_encode(tstframe, (unsigned char *)&known_plaintext);
	parse_ini_file();
	init_audiofiles();
	tch_init();

	bi = malloc(sizeof(*bi));
	GS_new(&d_gs_ctx_downlink);
	GS_new(&d_gs_ctx_uplink);
	if (timeslot_type == TST_FCCH_SCH_BCCH_CCCH) {
		d_gs_ctx_downlink.ts_ctx[0].type = TST_FCCH_SCH_BCCH_CCCH;
		d_gs_ctx_uplink.ts_ctx[0].type = TST_FCCH_SCH_BCCH_CCCH;
		d_gs_ctx_downlink.ts_ctx[1].type = TST_SDCCH8;
		d_gs_ctx_uplink.ts_ctx[1].type = TST_SDCCH8;
		d_gs_ctx_downlink.ts_ctx[2].type = TST_SDCCH8;
		d_gs_ctx_uplink.ts_ctx[2].type = TST_SDCCH8;
	} else {

		d_gs_ctx_downlink.ts_ctx[0].type = TST_FCCH_SCH_BCCH_CCCH_SDCCH4;
		d_gs_ctx_uplink.ts_ctx[0].type = TST_FCCH_SCH_BCCH_CCCH_SDCCH4;
		d_gs_ctx_downlink.ts_ctx[1].type = TST_TCHF;
		d_gs_ctx_uplink.ts_ctx[1].type = TST_TCHF;
		d_gs_ctx_downlink.ts_ctx[2].type = TST_TCHF;
		d_gs_ctx_uplink.ts_ctx[2].type = TST_TCHF;
	}
	d_gs_ctx_downlink.ts_ctx[3].type = TST_TCHF;
	d_gs_ctx_uplink.ts_ctx[3].type = TST_TCHF;
	d_gs_ctx_downlink.ts_ctx[4].type = TST_TCHF;
	d_gs_ctx_uplink.ts_ctx[4].type = TST_TCHF;
	d_gs_ctx_downlink.ts_ctx[5].type = TST_TCHF;
	d_gs_ctx_uplink.ts_ctx[5].type = TST_TCHF;
	d_gs_ctx_downlink.ts_ctx[6].type = TST_TCHF;
	d_gs_ctx_uplink.ts_ctx[6].type = TST_TCHF;
	d_gs_ctx_downlink.ts_ctx[7].type = TST_TCHF;
	d_gs_ctx_uplink.ts_ctx[7].type = TST_TCHF;
//	printf("Filename: %s\n",filename);

	f = open(filename, O_RDONLY);

	while (read(f, bi, sizeof(*bi)) == sizeof(*bi)) {
		gsm_fn2gsmtime(&tm, ntohl(bi->frame_nr));
		if (!use_statistics) {
			printf("\nHEX: ");
			hexdump(bi->bits,15);
			printf("dummy=%i\n", bi->flags & BI_FLG_DUMMY);
			printf("sacch=%i\n", bi->flags & BI_FLG_SACCH);
			printf("snr=%i\n", bi->snr);

			printf("(%6d = %.4u/%.2u/%.2u)\n", tm.fn, tm.t1, tm.t2, tm.t3);
		}

		ul = !!(ntohs(bi->band_arfcn) & ARFCN_UPLINK);
		if (ul) {
			res=GS_process(&d_gs_ctx_uplink, bi, d_KC, !uplink_cntr, !use_statistics);
			for(i=0; i<BURST_SIZE; i++)
                                bursts[uplink_cntr][i] = !!(bi->bits[i / 8] & (1 << (7 - (i % 8))));
			uplink_cntr++;
			uplink_cntr = uplink_cntr % 4;
		} else {
			res=GS_process(&d_gs_ctx_downlink, bi, d_KC, !downlink_cntr, !use_statistics);

			for(i=0; i<BURST_SIZE; i++) 
				bursts[downlink_cntr][i] = !!(bi->bits[i / 8] & (1 << (7 - (i % 8))));

			downlink_cntr++;
			downlink_cntr = downlink_cntr % 4;

		}

		/* Failed to decode, try known plaintext attack */
		if(res == -1 && (bi->snr > snr_cut) && !noattack) { // FIXME bad reception conditions destroy length guessing!
			// bi->chan_nr&7 is timeslot
			channel_type=get_chan_type(d_gs_ctx_downlink.ts_ctx[bi->chan_nr&7].type,tm.fn-3,&tmp); 
//			printf("DBG: Channel_type: %x, frame#: %d\n",channel_type,tm.fn);
			for(j=0;j<4;j++) {
				// -3 because we want first burst of frame after decoding failed
				gsm_fn2gsmtime(&tm, ntohl(bi->frame_nr) -3 + j);

				if (use_statistics) {
					// Add to the arrays & increment counters
					if (ul) {
						if ((channel_type & GSMTAP_CHANNEL_ACCH) != 0) { // 'a'
							memcpy(ACCH_array_ul[ACCH_length_ul],bursts[j],BURST_SIZE);
							ACCH_array_help_ul[ACCH_length_ul][0]=tm.fn;
							ACCH_array_help_ul[ACCH_length_ul][1]=((tm.t1<<11)|(tm.t3<<5)|tm.t2);
							ACCH_length_ul++;
						} else { // 'd'
							memcpy(DCCH_array_ul[DCCH_length_ul],bursts[j],BURST_SIZE);
							DCCH_array_help_ul[DCCH_length_ul][0]=tm.fn;
							DCCH_array_help_ul[DCCH_length_ul][1]=((tm.t1<<11)|(tm.t3<<5)|tm.t2);
							DCCH_length_ul++;
						}
					} else {
						if ((channel_type & GSMTAP_CHANNEL_ACCH) != 0) { // 'a'
							memcpy(ACCH_array_dl[ACCH_length_dl],bursts[j],BURST_SIZE);
							ACCH_array_help_dl[ACCH_length_dl][0]=tm.fn;
							ACCH_array_help_dl[ACCH_length_dl][1]=((tm.t1<<11)|(tm.t3<<5)|tm.t2);
							ACCH_length_dl++;
						} else { // 'd'
							memcpy(DCCH_array_dl[DCCH_length_dl],bursts[j],BURST_SIZE);
							DCCH_array_help_dl[DCCH_length_dl][0]=tm.fn;
							DCCH_array_help_dl[DCCH_length_dl][1]=((tm.t1<<11)|(tm.t3<<5)|tm.t2);
							DCCH_length_dl++;
						}
					}
				} else { // legacy code
					// w/o statistics just try XOR-ing of static given plaintext frame
					unsigned char pass = 0;
					if (ul && (uplink_cntr == 0)) {
						pass = 1;
					} else if (!ul && (downlink_cntr == 0)) {
						pass = 1;
					}
					if (ul != uplink) {
						pass = 0;
					}
					if(pass) {
						if (
							((targettype == 'a') && ((channel_type & GSMTAP_CHANNEL_ACCH) != 0)) ||
							((targettype == 'd') && ((channel_type & GSMTAP_CHANNEL_ACCH) == 0))
						) {
							printf("S %u %u: ", tm.fn, (tm.t1<<11)|(tm.t3<<5)|tm.t2);
							for(k=0;k<BURST_SIZE;k++)
								printf("%i",bursts[j][k] ^ known_plaintext[j][k]);
							printf("\n");
						}
					}
				}
			}
		}
	}
	
	// do this when we already know length
	if (use_statistics) {
		/* At first, print out transaction length.
		Other programs can for example guess if there is a SMS/call and prioritize us. */
		printf("Total encrypted SDCCH/DL frames: %i\n",DCCH_length_dl/4);

		/* Then, start guessing. */
		for(i=0; i<4 && guess_chantypes[i] != '\0'; i++) {
			if(use_adb) {
				guesser(guess_chantypes[i]-'0', aconn);
			}
			guesser(guess_chantypes[i]-'0', conn);
		}

/*		unsigned char tmp_frame[FRAME_SIZE]; // one frame (binary without redundancy)
		unsigned char tmp_array[4][BURST_SIZE]; // one frame (encoded 1 bit in one byte)
		unsigned char tmp_used_dcch_bursts[MAX_BURSTS_PER_FILE]; // to mark if we already used this ciphertext to produce keystream
		int x=0;

		memset(tmp_used_dcch_bursts,0,MAX_BURSTS_PER_FILE);
		if(guess_sdcch && !uplink) { // SDCCH/DL guesser - take care of len and positions in transaction
			sprintf(query, "SELECT x.position,y.plain FROM stats x join plaintexts y where "\
				"x.length=%i and x.id_plain=y.id and y.type=0 and x.probability > %f ORDER BY x.probability desc",DCCH_length/4,stats_threshold);
	
			sqlerr = SQLITE_OK;
			do { // DB can be locked sometimed (other process changing statistics)
		        	sqlerr = sqlite3_prepare_v2(conn, query, MAX_SQL_QUERY, &sqlres, &tail); 
				usleep(SQL_RETRY_TIME);
				if(sqlerr != SQLITE_OK) {
					fprintf(stderr,"stats select err: %s\n",sqlite3_errmsg(conn));
				}
			} while (sqlerr != SQLITE_OK);
	
		        while (SQLITE_ROW == sqlite3_step(sqlres)) {
				getplaintext(sqlite3_column_text(sqlres,1),tmp_frame);
				packet_encode(tmp_frame,(unsigned char *)&tmp_array);
	
				for (x=0;x<4;x++) {
					int index=(((sqlite3_column_int(sqlres,0)-1)*4)+x);
					printf("S %u %u: ",DCCH_array_help[index][0],DCCH_array_help[index][1] );
					for(k=0;k<BURST_SIZE;k++) {
						printf("%i",DCCH_array[index][k] ^ tmp_array[x][k]);
					}
					printf("\n");
					tmp_used_dcch_bursts[index]=1;
				}
			}
		}
		
		if(guess_sacch || uplink) { // SACCH guesser - no periodicity observed (yet), so bruteforce it ;)
			int chantype = 1; // SACCH/DL
			if(uplink) {
				chantype=3; // SACCH/UL
			}
			if(uplink && guess_sdcch) {
				chantype=2; // SDCCH/UL
			}
			sprintf(query, "SELECT y.plain FROM stats x join plaintexts y where "\
				"y.type=%i and x.id_plain=y.id and x.probability > %f ORDER BY x.probability desc;",chantype,stats_threshold);

			sqlerr = SQLITE_OK; // FIXME deduplicate this to a function
			do { // DB can be locked sometimed (other process changing statistics)
		        	sqlerr = sqlite3_prepare_v2(conn, query, MAX_SQL_QUERY, &sqlres, &tail); 
				usleep(SQL_RETRY_TIME);
				if(sqlerr != SQLITE_OK) {
					fprintf(stderr,"stats select err: %s\n",sqlite3_errmsg(conn));
				}
			} while (sqlerr != SQLITE_OK);
	
		        while (SQLITE_ROW == sqlite3_step(sqlres)) {
				getplaintext(sqlite3_column_text(sqlres,0),tmp_frame);
				packet_encode(tmp_frame,(unsigned char *)&tmp_array);

				int arr_pos;
				if(guess_sacch) { // SACCH both UL and DL
					for (arr_pos=0; arr_pos<ACCH_length/4; arr_pos++) { // iterate over the whole array of captured data
						for (x=0;x<4;x++) {
							int index=(((arr_pos)*4)+x);
							printf("S %u %u: ",ACCH_array_help[index][0],ACCH_array_help[index][1] );
							for(k=0;k<BURST_SIZE;k++) {
								printf("%i",ACCH_array[index][k] ^ tmp_array[x][k]);
							}
							printf("\n");
						}
					}
				} else { // SDCCH/UL
					for (arr_pos=0; arr_pos<DCCH_length/4; arr_pos++) { // iterate over the whole array of captured data
						for (x=0;x<4;x++) {
							int index=(((arr_pos)*4)+x);
							printf("S %u %u: ",DCCH_array_help[index][0],DCCH_array_help[index][1] );
							for(k=0;k<BURST_SIZE;k++) {
								printf("%i",DCCH_array[index][k] ^ tmp_array[x][k]);
							}
							printf("\n");
						}
					}
				}
			}
		}*/

		sqlerr = sqlite3_close(conn);

// too much requests, add by special option
/*
		for (x=0;x<DCCH_length;x++) {
			if (tmp_used_bursts[x] == 0) {
				printf("S %u %u: ",DCCH_array_help[x][0],DCCH_array_help[x][1] );
				for(k=0;k<BURST_SIZE;k++) {
					printf("%i",DCCH_array[x][k] ^ known_plaintext[x%4][k]);
				}
				printf("\n");
				// XXX select most common from DB
			
			}
		}
*/

	}

	close(f);
}

