#include <stdlib.h>

#include "common.h"

void parse_ini_file() {
	char inifile[1024];
	
	sprintf(inifile,"%s/.omgsm/config", getenv("HOME"));
	ini = iniparser_load(inifile);
	if (ini==NULL) {
		printf("cannot parse file\n");
		exit(1);
	}	
//	iniparser_dump(ini, stderr);
}

// input:  frame ( wireshark format )
// output: 4 bursts ( radio format )
void packet_encode(unsigned char *src, unsigned char *dst)
{
	unsigned char src_expand[184]; // 23*8  = length of input frame
	unsigned char parity[228]; // legth of ?
	unsigned char convol[456]; // length of 4 bursts
	int i,j;

	for(i=0; i<184; i++)
		src_expand[i] = !!(src[i / 8] & (1 << (i % 8)));
	memcpy(parity, src_expand, 184);
	parity_encode(parity, parity+184);
	memset(parity+224, 0, 4);
	conv_encode(parity, convol);
	interleave(convol, dst);
}

void hexdump(unsigned char *data, int size) {

	int i;

	for(i=0; i<size; i++) {
		printf("%.2x", data[i]);
	}
	printf("\n");
}

void bindump(unsigned char *data, int size) {

	int i;

	for(i=0; i<size; i++) {
		printf("%.1x", data[i]);
	}
	printf("\n");
}
