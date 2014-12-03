#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv) {

	int i;
	unsigned char frame[23];
	unsigned char bursts[4][114];

	if ((argc != 2) || (strlen(argv[1]) != 46)) {
		printf("Usage: %s burst\n", argv[0]);
		exit(1);
	}

        for(i = 0; i < 23; i++) {
                sscanf(argv[1], "%2hhx", &frame[i]);
		argv[1] += 2 * sizeof(char);
        }

//	hexdump(frame, 23);
	packet_encode(frame, (unsigned char *)bursts);
	bindump(bursts[0], 114);
	bindump(bursts[1], 114);
	bindump(bursts[2], 114);
	bindump(bursts[3], 114);
}
