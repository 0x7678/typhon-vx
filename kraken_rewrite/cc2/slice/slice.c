// reverse bites in ulong XXX optimize
ulong rev(ulong r) {
  ulong r1 = r;
  ulong r2 = 0;
  for (int j = 0; j < 64 ; j++ ) {
    r2 = (r2<<1) | (r1 & 0x01);
    r1 = r1 >> 1;
  }
  return r2;
}

/*
 Memory start:
 512 rft entries (8 colors * 40 tables = 320 + 192 padding)

 Kernel inputs:
 64 slices
 64 challenges
 64 current color indices + flags
 64 reserved
*/

kernel void krak(global uint *state, uint states) {
  
  /* kernel index */
  private size_t my = get_global_id(0); 

  /* pointer to memory "owned" by this kernel instance */
  private size_t myptr = 512+(my*256);

  private uint i,j,z,r;

  if (myptr < 1) {  // more kernels executed than needed
    break;
  }

  /* init slice algorithm variables */
  uint4 reg[16];
  uint4 ch[16];
  uint mask;

  // http://stackoverflow.com/questions/9788806/access-vector-type-opencl
  union {
    uint  a[4];
    uint4 v;
  } keystream;

  union {
    uint  a[64];
    uint4 v[16];
  } rf;

  uint clock1;
  uint clock2;
  uint clock3;
  uint iclk1;
  uint iclk2;
  uint iclk3;

  uint cr1r2;
  uint cr1r3;
  uint cr2r3;

  uint fb1;
  uint fb2;
  uint fb3;

  for(i=0; i<16; i++) {
    rf[i].x = 0;
    rf[i].y = 0;
    rf[i].z = 0;
    rf[i].w = 0;
  }

  /* copy data */
  for(i=0; i<16; i++) {
    reg[i].x = state_g[myptr+4*i];
    reg[i].y = state_g[myptr+4*i+1];
    reg[i].z = state_g[myptr+4*i+2];
    reg[i].w = state_g[myptr+4*i+3];
  }
  for(i=16; i<32; i++) {
    ch[i].x = state_g[myptr+4*i];
    ch[i].y = state_g[myptr+4*i+1];
    ch[i].z = state_g[myptr+4*i+2];
    ch[i].w = state_g[myptr+4*i+3];
  }
  for(i=0; i<32; i++) {
    int color_index = state_g[myptr+128+i] & 0x1ff;
    for(j=0; j<64; j++) {
      ulong bit = (state_g[color]&(1<<j))>>j;
      rf.a[j] = rf.a[j] | ( bit << i );
    }
  }

  /* run the A5/1 slice machine */
  for(z=0; z<1; z++) {

	/* apply RF */
	for(i=0; i<16; i++) {
		reg[i] = reg[i] ^ rf.v[i];
	}

	/* which instances reached distinguished point */
	uint res = reg[0].x | reg[0].y | reg[0].z | reg[0].w |
		   reg[1].x | reg[1].y | reg[1].z | reg[1].w |
		   reg[2].x | reg[2].y | reg[2].z | reg[2].w |
		   reg[3].x | reg[3].y;

	/* save prevlink (TODO) */

	/* increment color (TODO) */

	/* run 100 dummy clockings + 64 keystream clockings */
	for(r=0; r<164; r++) {
		/* compute clocking */
		cr1r2 = ~(reg[13].w ^ reg[8].z);
		cr1r3 = ~(reg[13].w ^ reg[3].x);
		cr2r3 = ~(reg[8].z  ^ reg[3].x);
		clock1 = cr1r2 | cr1r3;
		clock2 = cr1r2 | cr2r3;
		clock3 = cr1r3 | cr2r3;
		fb1 = (reg[11].y ^ reg[11].z ^ reg[11].w ^ reg[12].z) & clock1;
		fb2 = (reg[5].w ^ reg[6].x) & clock2;
		fb3 = (reg[0].x ^ reg[0].y ^ reg[0].z ^ reg[3].w) & clock3;

		/* do not clock instances in distinguished point */
		clock1 &= res;
		clock2 &= res;
		clock3 &= res;

		iclk1 = ~clock1;
		iclk2 = ~clock2;
		iclk3 = ~clock3;

		/* the shifting itself */
		for(i=0; i<5; i++) {
		  reg[i].x = (reg[i].x & iclk3) | (reg[i].y & clock3);
		  reg[i].y = (reg[i].y & iclk3) | (reg[i].z & clock3);
		  reg[i].z = (reg[i].z & iclk3) | (reg[i].w & clock3);
		  reg[i].w = (reg[i].w & iclk3) | (reg[i+1].x & clock3);
		}
		reg[5].x = (reg[5].x & iclk3) | (reg[5].y & clock3);
		reg[5].y = (reg[5].y & iclk3) | (reg[5].z & clock3);
		reg[5].z = (reg[5].z & iclk3) | fb3;

		reg[5].w = (reg[5].w & iclk2) | (reg[6].x & clock2);
		for(i=6; i<11; i++) {
		  reg[i].x = (reg[i].x & iclk2) | (reg[i].y & clock2);
		  reg[i].y = (reg[i].y & iclk2) | (reg[i].z & clock2);
		  reg[i].z = (reg[i].z & iclk2) | (reg[i].w & clock2);
		  reg[i].w = (reg[i].w & iclk2) | (reg[i+1].x & clock2);
		}
		reg[11].x = (reg[11].x & iclk2) | fb2;

		reg[11].y = (reg[11].y & iclk1) | (reg[11].z & clock1);
		reg[11].z = (reg[11].z & iclk1) | (reg[11].w & clock1);
		reg[11].w = (reg[11].w & iclk1) | (reg[12].x & clock1);
		for(i=12; i<15; i++) {
		  reg[i].x = (reg[i].x & iclk1) | (reg[i].y & clock1);
		  reg[i].y = (reg[i].y & iclk1) | (reg[i].z & clock1);
		  reg[i].z = (reg[i].z & iclk1) | (reg[i].w & clock1);
		  reg[i].w = (reg[i].w & iclk1) | (reg[i+1].x & clock1);
		}
		reg[15].x = (reg[15].x & iclk1) | (reg[15].y & clock1);
		reg[15].y = (reg[15].y & iclk1) | (reg[15].z & clock1);
		reg[15].z = (reg[15].z & iclk1) | (reg[15].w & clock1);
		reg[15].w = (reg[15].w & iclk1) | fb1;

		/* generate keystream */
		keystream.a[(r%100)&0x3f] = (reg[0].x ^ reg[5].w ^ reg[11].y);
	}
	/* check for challenge (TODO) */

	/* use computed keystream as input in next round */
	for(i=0; i<16; i++) {
		reg[i] = keystream.v[i];
	}
  }

  // copy data back
  for(i=0; i<16; i++) {
    state_g[myptr+4*i]   = reg[i].x;
    state_g[myptr+4*i+1] = reg[i].y;
    state_g[myptr+4*i+2] = reg[i].z;
    state_g[myptr+4*i+3] = reg[i].w;
  }
}
