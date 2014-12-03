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

#include <fcntl.h>

#include "common.h"



int main(int argc, char **argv) {

	char *tmp, *tmp2;
	int ret;
	char desc[1000];
	int comm;
	assignment transport;
	char pool_pipe[PATH_MAX] = DEF_PIPE_PATH;

	int ass_type;
	int arfcn;
        int chan_nr;
        int tsc;
        int maio;
        int hsn;

	strcpy(desc, argv[1]);
	transport.kanarek=0xdefeca7e;
	printf("%s\n", desc);
	if (desc[1] == '0') {
		sscanf(desc,"H%1u:0x%2x:%i:%1u:%x", &ass_type, &chan_nr, 
			&arfcn, &tsc, &transport.tmsi);
		printf("%i %x %i %i\n", ass_type, chan_nr, arfcn, tsc);
	} else {
		tmp=malloc(100);
		tmp2=malloc(100);
		ret=sscanf(desc,"H%1u:%2x:%i:%i:%1u:%x:%s", &ass_type,
			&chan_nr, &hsn, &maio, &tsc, &transport.tmsi, tmp);
		printf("ret=%i\n", ret);
		printf("%i %x %i %i %i\n", ass_type, chan_nr, hsn, maio, tsc);
//		printf("%s\n", tmp);
//		printf("%s\n", tmp2);
		transport.ma_len=0;
		while (1) {
			ret=sscanf(tmp,"%i,", &transport.ma[transport.ma_len]);
			if (ret < 1)
				break;
//			printf("%i\n", transport.ma[transport.ma_len]);
			transport.ma_len++;
			tmp=strchr(tmp, ',')+1;
		}
//		printf("%i\n", transport.ma_len);
	}
	printf("%08x\n", transport.tmsi);
	transport.sync_arfcn = 90; //45;
	transport.ass_type = ass_type;
	transport.arfcn = arfcn;
	transport.chan_nr = chan_nr;
	transport.tsc = tsc;
	transport.maio = maio;
	transport.hsn = hsn;

	printf("%i %x %i %i %i\n", transport.ass_type, transport.chan_nr, transport.hsn, transport.maio, transport.tsc);
	printf("%i %i %i %i %i\n",transport.ma[0],transport.ma[1],transport.ma[2], transport.ma[3], transport.ma[4], transport.ma[5]);
	printf("%i\n", transport.ma_len);

	comm = open(pool_pipe, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	write(comm, (char *) &transport, sizeof(transport));
	close(comm);
}
