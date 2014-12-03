#define _FILE_OFFSET_BITS 64

//#include "DeltaLookup.h"
//#include "NcqDevice.h"
//#include <iostream>
#include <stdio.h>
//#include <list>
#include <sys/time.h>
#include <assert.h>
#include <stdlib.h>

#include <inttypes.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <string.h>

#define BUFSIZE 140000

#define READ8()\
    bits = (mBitBuffer>>(mBitCount-8))&0xff;                 \
    mBitBuffer = (mBitBuffer<<8)|pBuffer[mBufPos++];

#define READ8safe()\
    bits = (mBitBuffer>>(mBitCount-8))&0xff;                 \
    if (mBufPos<4096) {                                      \
        mBitBuffer = (mBitBuffer<<8)|pBuffer[mBufPos++];     \
    }

#define READN(n)\
    bits = mBitBuffer>>(mBitCount-(n));         \
    bits = bits & ((1<<(n))-1);                 \
    mBitCount-=(n);                             \
    if(mBitCount<8) { \
        mBitBuffer = (mBitBuffer<<8)|pBuffer[mBufPos++];    \
        mBitCount+=8; \
    } 

#define DEBUG_PRINT 0

void error(char *msg) {
    perror(msg);
    exit(0);
}

uint64_t CompleteEndpointSearch(const void* pDataBlock, uint64_t here,
                                        uint64_t end);


static uint64_t kr02_whitening(uint64_t key)
{
    int i;
    uint64_t white = 0;
    uint64_t bits = 0x93cbc4077efddc15ULL;
    uint64_t b = 0x1;
    while (b) {
        if (b & key) {
            white ^= bits;
        }
        bits = (bits<<1)|(bits>>63);
        b = b << 1;
    }
    return white;
}

static uint64_t kr02_mergebits(uint64_t key)
{
    uint64_t r = 0ULL;
    uint64_t b = 1ULL;
    unsigned int i;

    for(i=0;i<64;i++) {
        if (key&b) {
            r |= 1ULL << (((i<<1)&0x3e)|(i>>5));
        }
        b = b << 1;
    }
    return r;
}

uint64_t ApplyIndexFunc(uint64_t start_index, int bits)
{
    uint64_t w = kr02_whitening(start_index);
    start_index = kr02_mergebits((w<<bits)|start_index);
    return start_index;
}



/*bool DeltaLookup::mInitStatics = false;
unsigned short DeltaLookup::mBase[256];
unsigned char DeltaLookup::mBits[256];*/

int mBlockIndex[40][10227760+100000];
uint64_t mPrimaryIndex[40][39952+1000];

//const char * files[2] = { "/root/gsm/myidx/220.idx", "/root/gsm/indexes/380.idx" };
//const uint64_t offsets[2] = {81849336, 102347869};

const char * files[40] = { "/root/gsm/indexes/380.idx", "/root/gsm/myidx/220.idx",
"/root/gsm/indexes/100.idx",
"/root/gsm/indexes/108.idx",
"/root/gsm/indexes/116.idx",
"/root/gsm/indexes/124.idx",
"/root/gsm/indexes/132.idx",
"/root/gsm/indexes/140.idx",
"/root/gsm/indexes/148.idx",
"/root/gsm/indexes/156.idx",
"/root/gsm/indexes/164.idx",
"/root/gsm/indexes/172.idx",
"/root/gsm/indexes/180.idx",
"/root/gsm/indexes/188.idx",
"/root/gsm/indexes/196.idx",
"/root/gsm/indexes/204.idx",
"/root/gsm/indexes/212.idx",
"/root/gsm/indexes/230.idx",
"/root/gsm/indexes/238.idx",
"/root/gsm/indexes/250.idx",
"/root/gsm/indexes/260.idx",
"/root/gsm/indexes/268.idx",
"/root/gsm/indexes/276.idx",
"/root/gsm/indexes/292.idx",
"/root/gsm/indexes/324.idx",
"/root/gsm/indexes/332.idx",
"/root/gsm/indexes/340.idx",
"/root/gsm/indexes/348.idx",
"/root/gsm/indexes/356.idx",
"/root/gsm/indexes/364.idx",
"/root/gsm/indexes/372.idx",
"/root/gsm/indexes/388.idx",
"/root/gsm/indexes/396.idx",
"/root/gsm/indexes/404.idx",
"/root/gsm/indexes/412.idx",
"/root/gsm/indexes/420.idx",
"/root/gsm/indexes/428.idx",
"/root/gsm/indexes/436.idx",
"/root/gsm/indexes/492.idx",
"/root/gsm/indexes/500.idx"  };
const uint64_t offsets[40] = {102347869, 81849336,
0,
20461178,
112574826,
102336184,
92077095,
92105934,
30702472,
30688927,
40931967,
0,
40924796,
51169435,
71618441,
61409247,
102343350,
61407630,
10229859,
30695679,
51162401,
112576721,
10232259,
61385698,
71639236,
81873709,
10228856,
20459800,
71641995,
81874248,
30695489,
20461339,
92105934,
0,
112596312,
51153680,
20467386,
10233293,
0,
40935940
};

const char * devs[40] = {
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500600000463", //220
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500600000463", //380
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500600000463",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500600000463",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02730235500600001031",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02730235500600001031",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500600000463",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500300000310",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02730235500600001031",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716033500100000013",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02730235500600001031",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02730235500600001031",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500600000463",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500300000310",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500600000463",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500300000310",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500300000310",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02730235500600001031",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716033500100000013",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500300000310",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02730235500600001031",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500300000310",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500300000310",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500600000463",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500300000310",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02730235500600001031",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500600000463",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716033500100000013",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02730235500600001031",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500300000310",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500600000463",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500300000310",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02730235500600001031",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500300000310",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500600000463",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500600000463",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02730235500600001031",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02730235500600001031",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716033500100000013",
"/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500300000310" };

int mNumBlocks[40];
unsigned long mStepSize[40];
int64_t mLowEndpoint[40];
int64_t mHighEndpoint[40];
int64_t mBlockOffset;

    static unsigned short mBase[256];
    static unsigned char mBits[256];

int mInitStatics = 0;

void load_idx() {
    for(int idx=0; idx<40; idx++) {
	    /* Load index - compress to ~41MB of alloced memory */
	    FILE* fd = fopen(files[idx],"rb");
	    if (fd==0) {
		printf("Could not open %s for reading.\n", "f");
	    }
	    assert(fd);
	    fseek(fd ,0 ,SEEK_END );
	    long size = ftell(fd);
	    unsigned int num = (size / sizeof(uint64_t))-1;
	    fseek(fd ,0 ,SEEK_SET );
	    size_t alloced = num*sizeof(int)+(num/256)*sizeof(int64_t);
	    fprintf(stderr, "Allocated %i bytes: %i\n",alloced,idx);
	    mNumBlocks[idx] = num;
	//    assert(mBlockIndex);
	    uint64_t end;
	    mStepSize[idx] = 0xfffffffffffffLL/(num+1);
	    int64_t min = 0;
	    int64_t max = 0;
	    int64_t last = 0;
	    for(int bl=0; bl<num; bl++) {
		size_t r = fread(&end,sizeof(uint64_t),1,fd);
		assert(r==1);
		int64_t offset = (end>>12)-last-mStepSize[idx];
		last = end>>12;
		if (offset>max) max = offset;
		if (offset<min) min = offset;
		if (offset >= 0x7fffffff || offset <=-0x7fffffff) {
		    fprintf(stderr,"index file corrupt: %s\n", "F");
		    exit(1);
		}
		mBlockIndex[idx][bl]=offset;
		if ((bl&0xff)==0) {
		    mPrimaryIndex[idx][bl>>8]=end;
		}
	    }
	    mBlockIndex[idx][num] = 0x7fffffff; /* for detecting last index */
	    // printf("%llx %llx %llx\n", min,max,mPrimaryIndex[1]);

	    mLowEndpoint[idx] = mPrimaryIndex[idx][0];
	    size_t r=fread(&mHighEndpoint[idx],sizeof(uint64_t),1,fd);
	    assert(r==1);
	    fclose(fd);
	    mBlockOffset=0ULL;
    }
//    mDevice = dev;

    if (!mInitStatics) {
        // Fill in decoding tables
        int groups[] = {0,4,126,62,32,16,8,4,2,1};
        int gsize = 1;
        unsigned short base = 0;
        int group = 0;
        for (int i=0;i<10;i++) {
            for (int j=0; j<groups[i]; j++) {
                mBase[group] = base;
                mBits[group] = i;
                base += gsize;
                group++;
            }
            gsize = 2 * gsize;
        }
        // The rest should be unused 
        assert(group<256);
        mInitStatics = 1;
    }
}

/*DeltaLookup::~DeltaLookup()
{
    delete [] mBlockIndex;
    delete [] mPrimaryIndex;
}*/

#define QUEUE_SIZE 320
#define MAX_MMAP_SIZE 1800000000000
#define DEVICE "/dev/disk/by-id/scsi-SATA_ADATA_SX900_02716081500600000463"

uint64_t MineABlock(long blockno, uint64_t here, uint64_t target, int tbl) {
///    char* storage = (char *)mmap(NULL, MAX_MMAP_SIZE, PROT_READ, MAP_PRIVATE|MAP_FILE, DEVICE, 0);
//    int offset = 81849336;
    unsigned char mBuffer[4096];
    int mDevice = open(devs[tbl],O_RDONLY);
    blockno += offsets[tbl];
    //printf("Block %i, here %llx, target %llx\n", blockno, here,target);
    lseek(mDevice, blockno*4096, SEEK_SET );
    size_t r = read(mDevice,mBuffer,4096);
    assert(r==4096);
    
//    printf("%i %x%x%x%x\n", blockno, mBuffer[0], mBuffer[1], mBuffer[100], mBuffer[200]);
    close(mDevice);
    return(CompleteEndpointSearch(mBuffer, here, target));

}
#ifdef hektor
void NCQProc() {
    unsigned char core[4];

        //mmap whole device
        // XXX not optimal, size is hardcoded
        char* storage = (char *)mmap64(NULL, MAX_MMAP_SIZE, PROT_READ,
                                          MAP_PRIVATE|MAP_FILE, mDevice,
                                          0);

    while (mRunning) {
        usleep(200); //XXX was 500
        sem_wait(&mMutex);
        /* schedule requests */
        while ((mFreeMap>=0)&&(mRequests.size()>0))
        {
            int free = mFreeMap;
            mFreeMap = mMappings[free].next_free;
            request_t qr = mRequests.front();
            mRequests.pop();
            mMappings[free].req = qr.req;
            mMappings[free].blockno = qr.blockno;
/*            mMappings[free].addr = mmap64(NULL, 4096, PROT_READ,
                                          MAP_PRIVATE|MAP_FILE, mDevice,
                                          qr.blockno*4096);*/
           mMappings[free].addr = storage+((qr.blockno*4096));


            // printf("Mapped %p %lli\n", mMappings[free].addr, qr.blockno);
            madvise(mMappings[free].addr,4096,MADV_WILLNEED);
        }
        sem_post(&mMutex);

        /* Check request */
        for (int i=0; i<QUEUE_SIZE; i++) { // XXX was 32
            if (mMappings[i].addr) {
                mincore(mMappings[i].addr,4096,core);
                if (core[0]&0x01) {
                    // Debug disk access
                    // printf("%c",mDevC);
                    // fflush(stdout);
                    /* mapped & ready for use */
                    mMappings[i].req->processBlock(mMappings[i].addr);
//                    munmap(mMappings[i].addr,4096);
                    mMappings[i].addr = NULL;
                    /* Add to free list */
                    sem_wait(&mMutex);
                    mMappings[i].next_free = mFreeMap;
                    mFreeMap = i;
                    sem_post(&mMutex);
                } else {
                    madvise(mMappings[i].addr,4096,MADV_WILLNEED);
                }
            }
        }
    }
}
#endif

uint64_t StartEndpointSearch(uint64_t end, uint64_t blockstart, int tbl)
{
//printf("end=%llx, blockstart=%llx\n", end, blockstart);
    if (end<mLowEndpoint[tbl]) return 0ULL;
    if (end>mHighEndpoint[tbl]) return 0ULL;

    uint64_t bid = (end>>12) / mStepSize[tbl];
    unsigned int bl = ((unsigned int)bid)/256;

    // Methinks the division has been done by float, and may 
    // have less precision than required
    while (bl && (mPrimaryIndex[tbl][bl]>end)) bl--;

    uint64_t here = mPrimaryIndex[tbl][bl];
    int count = 0;
    bl = bl *256;
    uint64_t delta = (mStepSize[tbl] + mBlockIndex[tbl][bl+1])<<12;

#if DEBUG_PRINT
    printf("here: %llx bl: %llu\n", here, bl);
#endif

	// XXX 41MB block => 42991616, ble (41 * 1024 * 1024)
    while(((here+delta)<=end) && (bl<mNumBlocks[tbl]+1)) {
        here+=delta;
        bl++;
        count++;
#if DEBUG_PRINT
    printf("here: %llx bl: %llu\n", here, bl);
#endif
        delta = (mStepSize[tbl] + mBlockIndex[tbl][bl+1])<<12;
    }

#if DEBUG_PRINT
    printf("%i block (%i)\n", bl, count);
#endif

    blockstart = here; // set first in case of sync loading 
    uint64_t re = MineABlock(bl,here,end,tbl);
//    mDevice->Request(req, (uint64_t)bl+mBlockOffset );
    return re;
}

void main(int argc, char **argv) {
	int sockfd, portno, n;
	struct sockaddr_in serveraddr;
	struct hostent *server;
	uint64_t buf[BUFSIZE];

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
		error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname("localhost");
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as\n");
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(6666);

    /* connect: create a connection with the server */
    if (connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) 
      error("ERROR connecting");

	strcpy(buf,"getdps\n");
	n = write(sockfd, buf, strlen(buf));
    if (n < 0) 
      error("ERROR writing to socket");
for(int iter=0; iter<10000; iter++) {
	printf("DELTA: PASS: %i\n",iter);
	int tot=0;
	while(tot<130568) {
		n = read(sockfd, buf+tot/8, 4096);
		tot += n;
		printf("rx tot=%i\n",tot);
		fflush(stdout);
	}
//	recv_full(sockfd, buf, 130568);
//	printf("n=%i\n", n);
/*	for(int i=0; i<BUFSIZE; i++) {
		printf("%02x", buf[i]&0xff);
		if(i%64 == 0) printf("\n");
	}
	exit(42);*/

	if(iter==0) {
		load_idx();
	}
	uint64_t bnum = buf[0];
	printf("bnum=%i\n",bnum);

	for(int tbl=0; tbl<40; tbl++) {
		for(int i=1; i<408; i++) {
			uint64_t sta = 0;
			uint64_t re = 0;
			int j = i+(tbl*408);
			uint64_t tg = buf[j];
			//tg= 0xdfd05a8b899b6000ULL;
			//printf("Search for %llx, table %i, i %i, j %i\n", tg, tbl, i, j);
			re = StartEndpointSearch(tg, sta, tbl);
			if(re) {
				re=ApplyIndexFunc(re, 34);
			}
			buf[j] = re;
			//printf("re: %llx\n", re);
		}
	}

	n = write(sockfd, "retsta", 6);
	tot=0;
	while(tot<130568) {
		n = write(sockfd, buf+tot/8, (130568-tot));
		tot += n;
		printf("tx tot=%i\n",tot);
		fflush(stdout);
	}
	sleep(1);
}


}

#define DEBUG_PRINT 0

uint64_t CompleteEndpointSearch(const void* pDataBlock, uint64_t here,
                                        uint64_t end) {
    const unsigned char* pBuffer = (const unsigned char*)pDataBlock;
    unsigned int mBufPos = 0;
    unsigned int mBitBuffer = pBuffer[mBufPos++];
    unsigned int mBitCount = 8;
    unsigned char bits;
    uint64_t index;
    uint64_t tmp, result;
    uint64_t delta;


    // read generating index for first chain in block 
    READ8();
    tmp = bits;
    READ8();
    tmp = (tmp<<8)|bits;
    READ8();
    tmp = (tmp<<8)|bits;
    READ8();
    tmp = (tmp<<8)|bits;
    READN(2);
    tmp = (tmp<<2)|bits;

#if DEBUG_PRINT
    printf("%llx %llx\n", here, tmp);
#endif

    if (here==end) {
        result = tmp;
        return result;
    }

    for(;;) {
        int available = (4096-mBufPos)*8 + mBitCount;
        if (available<51) {
#if DEBUG_PRINT
            printf("End of block (%i bits left)\n", available);
#endif
            break;
        }
        READ8();
        if (bits==0xff) {
            if (available<72) {
#if DEBUG_PRINT
                printf("End of block (%i bits left)\n", available);
#endif
                break;
            }
            // Escape code 
            READ8();
            tmp = bits;
            READ8();
            tmp = (tmp<<8)|bits;
            READ8();
            tmp = (tmp<<8)|bits;
            READ8();
            tmp = (tmp<<8)|bits;
            READ8();
            tmp = (tmp<<8)|bits;
            READ8();
            tmp = (tmp<<8)|bits;
            READ8();
            tmp = (tmp<<8)|bits;
            READ8safe();
            tmp = (tmp<<8)|bits;
            delta = tmp >> 34;
            index = tmp & 0x3ffffffffULL;
        } else {
            unsigned int code = bits;
            unsigned int rb = mBits[code];
            //printf("%02x - %i - %x ",code,rb,mBase[code]);
            delta = mBase[code];
            unsigned int d2 = 0;
            if (rb>=8) {
                READ8();
                d2 = bits;
                rb-=8;
            }
            if (rb) {
                READN(rb);
                d2 = (d2<<rb)|bits;
            }
            //printf("%llx %x\n",delta,d2);
            delta+=d2;
            READ8();
            delta = (delta<<8)|bits;
            READN(2);
            delta = (delta<<2)|bits;

            READN(1);
            uint64_t idx = bits;
            READ8();
            idx = (idx<<8)|bits;
            READ8();
            idx = (idx<<8)|bits;
            READ8();
            idx = (idx<<8)|bits;
            READ8safe();
            index = (idx<<8)|bits;
        }
        here += delta<<12;
#if DEBUG_PRINT
        printf("%llx %llx\n", here, index);
#endif
        if (here==end) {
           result = index;
           return result; 
        }

        if (here>end) {
#if DEBUG_PRINT
            printf("passed: %llx %llx\n", here, end);
#endif
            break;
        }
    }

    return 0;
}

