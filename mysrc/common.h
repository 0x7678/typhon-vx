#include "iniparser/src/iniparser.h"
#include "iniparser/src/dictionary.h"
#include <stdint.h>
#include <limits.h>
#include <assert.h>
#include <time.h>

#include <osmocom/gsm/gsm48_ie.h>


dictionary *ini ;

void parse_ini_file();
void packet_encode(unsigned char *src, unsigned char *dst);
void hexdump(unsigned char *data, int size);
void bindump(unsigned char *data, int size);


extern int mode;
extern char pool_pipe[PATH_MAX];

#define MA_MAX_LEN 256

// global variables (slave/master)
#define MODE_STANDALONE 0
#define MODE_MASTER 1
#define MODE_SLAVE 2

#define DEF_PIPE_PATH "/tmp/gsm_fifo"

#define BADCOUNT_RELEASE 12
// microseconds
#define SQL_RETRY_TIME	1000
// was 60 originally, but too many bursts joined together
#define MAX_SQL_QUERY	1000
// max records returned in some parts of dirty code

#define BURST_SIZE 114
// in bits

#define FRAME_SIZE 23 
// in bytes

#define MAX_BURSTS_PER_FILE 1024

#define CHAN_EARLY_ABANDON_CNT 16

// structure for master-slave communication
typedef struct {
        int             ass_type;
/*
0 - non-hopping imm_ass (l1ctl_tx_dm_est_req_h0)
1 - hopping imm_ass (l1ctl_tx_dm_est_req_h1)
2 - nonhopping ass_cmd (l1ctl_tx_dm_est_req_h0)
3 - hopping ass_cmd (l1ctl_tx_dm_est_req_h1)
*/
        uint16_t        arfcn;
        uint8_t         chan_nr;
        uint8_t         tsc;
        uint8_t         maio;
        uint8_t         hsn;
        uint16_t        ma[MA_MAX_LEN];
        int             ma_len;

// For freq/timesynchronization
	uint16_t	sync_arfcn;
// For creating .dat file
	uint32_t	tmsi;

// for saving MCC/MNC and CELLId
	uint32_t	b_mcnc;
	uint32_t	b_cellid;
	uint32_t	kanarek;

} assignment;

/* FIXME - Should be moved to custom_ms */
enum dch_state_t {
	DCH_NONE,
	DCH_WAIT_EST,
	DCH_ACTIVE,
	DCH_WAIT_REL,
};

/* FIXME - Should be moved to custom_ms */
typedef struct {
	int			has_si1;
	int			ccch_mode;

	enum dch_state_t	dch_state;
	uint8_t			dch_nr;
	int			dch_badcnt;

	FILE *			fh;

	struct gsm_sysinfo_freq	cell_arfcns[1024];
} app_state_t;

