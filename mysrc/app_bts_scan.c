/* "Application" code of the layer2/3 stack */

/* (C) 2010 by Andreas Eversberg <jolly@eversberg.eu>
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

#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>

#include <osmocom/bb/common/osmocom_data.h>
#include <osmocom/bb/common/l1ctl.h>
#include <osmocom/bb/common/l23_app.h>
#include <osmocom/bb/common/logging.h>
#include <osmocom/bb/common/gps.h>
#include <osmocom/bb/misc/cell_log.h>

#include <osmocom/core/talloc.h>
#include <osmocom/core/utils.h>

#include <l1ctl_proto.h>

#include <osmocom/bb/common/l1l2_interface.h>
#include <osmocom/bb/common/sap_interface.h>
#include <osmocom/bb/misc/layer3.h>
#include <osmocom/bb/common/logging.h>

#include <osmocom/core/msgb.h>
#include <osmocom/core/select.h>
#include <osmocom/core/linuxlist.h>
#include <osmocom/core/gsmtap_util.h>
#include <osmocom/core/gsmtap.h>


extern struct log_target *stderr_target;
void *l23_ctx;
int (*l23_app_work) (struct osmocom_ms *ms) = NULL;
int (*l23_app_exit) (struct osmocom_ms *ms) = NULL;
struct gsmtap_inst *gsmtap_inst;

int RACH_MAX = 2;

int _scan_work(struct osmocom_ms *ms)
{
	return 0;
}

int _scan_exit(struct osmocom_ms *ms)
{
	/* in case there is a lockup during exit */
	signal(SIGINT, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);

	scan_exit();

	return 0;
}

int l23_app_init(struct osmocom_ms *ms)
{
	int rc;

	srand(time(NULL));

	l23_app_work = _scan_work;
	l23_app_exit = _scan_exit;

	rc = scan_init(ms);
	if (rc)
		return rc;

	l1ctl_tx_reset_req(ms, L1CTL_RES_T_FULL);
	printf("Mobile initialized, please start phone now!\n");
	return 0;
}

static int l23_cfg_supported()
{
	return L23_OPT_TAP | L23_OPT_DBG;
}

static int l23_getopt_options(struct option **options)
{
	static struct option opts [] = {
		{"rach", 1, 0, 'r'},
	};

	*options = opts;
	return ARRAY_SIZE(opts);
}

static int l23_cfg_print_help()
{
	printf("\nApplication specific\n");
	printf("  -r --rach RACH	Nr. of RACH bursts to send.\n");

	return 0;
}

static int l23_cfg_handle(int c, const char *optarg)
{
	switch (c) {
	case 'r':
		RACH_MAX = atoi(optarg);
		break;
	}
	return 0;
}

static struct l23_app_info info = {
	.copyright	= "Copyright (C) 2010 Andreas Eversberg\n",
	.getopt_string	= "r:",
	.cfg_supported	= l23_cfg_supported,
	.cfg_getopt_opt = l23_getopt_options,
	.cfg_handle_opt	= l23_cfg_handle,
	.cfg_print_help	= l23_cfg_print_help,
};

struct l23_app_info *l23_app_info()
{
	return &info;
}
