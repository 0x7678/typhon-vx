/* Turn A5/1 machine "steps" times
   start: initial LFSR states
   steps: number of iterations
   retstream: true: return keystream
              false: return LFSR states

*/ 
ulong a51(ulong start, int steps, char retstream) {
  /* Magic. Don't touch. */
  // a5/1 magic constants
  unsigned int lfsr1 = start & 0x7ffff;
  unsigned int lfsr2 = (start>>19) & 0x3fffff;
  unsigned int lfsr3 = (start>>41) & 0x7fffff;

//  int bits = 0;
  ulong stream = 0;

  for (int i=0; i<steps; i++) {
    /* Majority count */
    int count = ((lfsr1>>8)&0x01);
    count += ((lfsr2>>10)&0x01);
    count += ((lfsr3>>10)&0x01);
    count = count >> 1;

    unsigned int bit = ((lfsr1>>18)^(lfsr2>>21)^(lfsr3>>22))&0x01;
    stream = (stream<<1) | bit;

    /* Clock the different lfsr */
    if (((lfsr1>>8)&0x01)==count) {
      unsigned int val = (lfsr1&0x52000)*0x4a000;
      val ^= lfsr1<<(31-17);
      lfsr1 = 2*lfsr1 | (val>>31);
    }
    if (((lfsr2>>10)&0x01)==count) {
      unsigned int val = (lfsr2&0x300000)*0xc00;
      lfsr2 = 2*lfsr2 | (val>>31);
    }
    if (((lfsr3>>10)&0x01)==count) {
      unsigned int val = (lfsr3&0x500080)*0x1000a00;
      val ^= lfsr3<<(31-21);
      lfsr3 = 2*lfsr3 | (val>>31);
    }
  }

  if(retstream) {
    return stream; //return keystream
  } else {
    // return a5/1 internal state
    lfsr1 = lfsr1 & 0x7ffff;
    lfsr2 = lfsr2 & 0x3fffff;
    lfsr3 = lfsr3 & 0x7fffff;

    ulong res = (ulong)lfsr1 | ((ulong)lfsr2<<19) | ((ulong)lfsr3<<41);

    return res;
  }
}

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




// ---------- kernel

/*
INPUTS: 

OUTPUTS: rewriting input values

This function can work in 2 modes:
 - if challenge 0: computing a5/1 chain to distinguished point 
 - if challenge is not 0 : getting previous cipherstate to provided cipherstate
*/

kernel void krak(global ulong *state_g, uint states) {
  
  // kernel index
  private size_t my = get_global_id(0); 

  // pointer to memory "owned" by this kernel instance
  private size_t myptr = my * 12; 
  // (all needed data should fit into 12 ulongs)

  // local vars
  private ulong state, reg;

  // reduction function table (colors)
  private ulong rft[8];

  // current_color, maximal color, challenge we are looking for.
  private ulong c_color, s_color, challenge;

  // previous state
  private ulong prevlink;

  private uint gotit = 0;

  private uint i;

  // copy data 
  if (myptr < states) {
    // more kernels executed than needed

    // offsets are hardcoded for now
    state = state_g[myptr];
    for(i = 0; i<8; i++) {
      rft[i] = state_g[myptr+i+1];
    }
    c_color = state_g[myptr+9]&0x0f;
    s_color = state_g[myptr+10]&0x0f;
    challenge = state_g[myptr+11];
    //printf("states %i myptr %i\n", states, myptr);

    reg = state;
    for (i = 0; i < 40000; i++) {
      // do NUM rounds (preparation for bitslicing)
      // we don't know how to break gpu kernel execution
      // so our parent need to handle this

      // apply round (transition, advance) function
      reg = rft[c_color] ^ reg;

      // check for Distinguished Point
      if(reg>>52 == 0) {
        c_color++;
        if(c_color > s_color) { // missed target
	  // tried all the colors
          state_g[myptr+9] = c_color|0x8000000000000000ULL; // report finished
          state_g[myptr] = rev(reg);
          gotit=1;
       	  break;
        }
      }
      //printf("in %llx\n",reg);
      prevlink=reg;
      reg = a51(reg,100,0); // tables are computed with 100 dummy round
      reg = a51(reg,64,1);  // 64 bit are needed for next input
      //printf("ks %llx\n",reg);
      if(challenge && (challenge == reg)) {
        //printf("#%x FOUND %llx <- %llx <-> %llx\n", my, reg, prevlink, rev(prevlink));
	state_g[myptr] = rev(prevlink);
        state_g[myptr+9] = c_color|0xC000000000000000ULL;
        gotit=1;
	break;
      }
    }

    // copy data to output (we did not get to DP)
    if(!gotit) {
      state_g[myptr] = reg;
      state_g[myptr+9] = c_color|(state_g[myptr+9]&0xF000000000000000ULL);
    }
  }

}
//
//// obsolete
//// ----------
//  // async copy to local area - is it useful for ATI?
//  // TODO this cannot work as local definitions must be static and BLOCK_SIZE
//  // is here determined in runtime
//  ///local ulong state_l[BLOCK_SIZE];
//  // TODO not working yet
//  /*private event_t ev_copy = async_work_group_copy(
//          (local ulong *) state_l,  
//          (global ulong *) &state_g[BLOCK_ID*BLOCK_SIZE],  
//          (size_t) (BLOCK_SIZE),  
//          (event_t) 0);
//  wait_group_events(1, &ev_copy);*/
//
//
//      /*	//reg =shiftLFSRs(reg)
//      // gen stream of 64 bits and rewrite existing state
//      for (ulong j = A_OUT; j > 0; j >>= 1) {
//          reg = shiftLFSRs(reg);
//          // xor outputs
//          uint result = BITXOR(reg & ALL_OUT);
//          // update state
//          //state_out = inactive ? state_out : (result ? (state_out | j) : (state_out & ~j));
//          state_out = inactive ? state_out : (state_out & ~j);
//      }*/
//  //state_g[1] = 0xFFFFFF;
//  //assync copy to local area - is it useful for ATI?
//  // TODO not working yet
//  //barrier(CLK_LOCAL_MEM_FENCE);
//  /*ev_copy = async_work_group_copy(
//          (global ulong *) &state_g[BLOCK_ID*BLOCK_SIZE],  
//          (local ulong *) state_l,
//          (size_t) (BLOCK_SIZE),
//          (event_t) 0);
//  wait_group_events(1, &ev_copy);*/
//
//// ---------- macros to derive linear indices of kernels
//#define BLOCK_SIZE (get_local_size(0)*get_local_size(1)*get_local_size(2))
//#define BLOCK_COUNT (get_num_groups(0)*get_num_groups(1)*get_num_groups(2))
//#define LOCAL_ID (get_local_id(0) + get_local_size(0)*(get_local_id(1) + get_local_size(1)*get_local_id(2)))
//#define BLOCK_ID (get_group_id(0) + get_num_groups(0)*(get_group_id(1) + get_num_groups(1)*get_group_id(2)))
//#define GLOBAL_ID (get_global_id(0) + get_global_size(0)*(get_global_id(1) + get_global_size(1)*get_global_id(2)))
////#define GLOBAL_ID (LOCAL_ID + (BLOCK_ID * BLOCK_SIZE)) // ugly
//// ----------
//
//
//// XXX not used?
//// ---------- popcount definition
//// if we don`t have C1.2 or specific extension - popcout is NOT builtin
//#pragma OPENCL EXTENSION cl_amd_popcnt : enable
//#if ! (defined CL_VERSION_1_2 || defined cl_amd_popcnt)
//#if defined POPCOUNT_NAIVE
//uint popcount(private ulong x) {
//    //This is a naive implementation, shown for comparison,
//    //and to help in understanding the better functions.
//    //It uses 24 arithmetic operations (shift, add, and).
//    x = (x & 0x5555555555555555 ) + ((x >>  1) & 0x5555555555555555 ); //put count of each  2 bits into those  2 bits 
//    x = (x & 0x3333333333333333 ) + ((x >>  2) & 0x3333333333333333 ); //put count of each  4 bits into those  4 bits 
//    x = (x & 0x0f0f0f0f0f0f0f0f ) + ((x >>  4) & 0x0f0f0f0f0f0f0f0f ); //put count of each  8 bits into those  8 bits 
//    x = (x & 0x00ff00ff00ff00ff ) + ((x >>  8) & 0x00ff00ff00ff00ff ); //put count of each 16 bits into those 16 bits 
//    x = (x & 0x0000ffff0000ffff ) + ((x >> 16) & 0x0000ffff0000ffff ); //put count of each 32 bits into those 32 bits 
//    x = (x & 0x00000000ffffffff ) + ((x >> 32) & 0x00000000ffffffff ); //put count of each 64 bits into those 64 bits 
//    return x;
//}
//#elif defined POPCOUNT_ITERATED
//inline uint popcount(private ulong x) {
//    //This is better when most bits in x are 0
//    //It uses 3 arithmetic operations and one comparison/branch per "1" bit in x.
//    uint count;
//    for (count=0; x; count++)
//        x &= x-1;
//    return count;
//}
//#elif defined POPCOUNT_FAST
//uint popcount(private ulong x) {
//    //This uses fewer arithmetic operations than any other known  
//    //implementation on machines with fast multiplication.
//    //It uses 12 arithmetic operations, one of which is a multiply.
//    x -= (x >> 1) & 0x5555555555555555; //put count of each 2 bits into those 2 bits
//    x = (x & 0x3333333333333333) + ((x >> 2) & 0x3333333333333333); //put count of each 4 bits into those 4 bits 
//    x = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0f; //put count of each 8 bits into those 8 bits 
//    return (x * 0x0101010101010101)>>56; //returns left 8 bits of x + (x<<8) + (x<<16) + (x<<24) + ... 
//}
//#else
//inline uint popcount(private ulong x) {
//    // this is superfast
//    return __builtin_popcount(x);
//}
//#endif
//#endif
//// ----------
//
//// XXX not used?
//// ---------- some useful bitstring operations (NOT bitwise)
//// !0x0000 -> 1
//// !0x0010 -> 0
//#define BITNOR(x) (!(x)) // (x == 0)
//// !!0x0000 -> !1 -> 0
//// !!0x0010 -> !0 -> 1
//#define BITOR(x) (!BITNOR(x)) // (x != 0)
//// !~0xffff -> !0x0000 -> 1
//// !~0x0f00 -> !0xf0ff -> 0
//#define BITAND(x) (BITNOR(~(x))) // (~x == 0)
//// !!~0xffff -> !!0x0000 -> !1 -> 0
//// !!~0x0100 -> !!0xfeff -> !0 -> 1
//#define BITNAND(x) (!BITAND(x)) // (~x != 0)
//// popcount counts "1" bits so XOR is then LSB of "1" count
//// popcount(0x0000) & 1 -> 0x0000 & 1 -> 0
//// popcount(0x3010) & 1 -> 0x0003 & 1 -> 1
//// popcount(0x3000) & 1 -> 0x0002 & 1 -> 0
//#define BITXOR(x) (popcount(x) & 1)
//// ---------- 
//
//// ---------- LFSRs state (A - 23bit, B - 22bit, C - 19bit)
//// AAAAAAAA AAAAAAAA AAAAAAAB BBBBBBBB BBBBBBBB BBBBBCCC CCCCCCCC CCCCCCCC
//// <out------------------in<<out------------------in<<out--------------in<
//#define A_MASK      0xFFFFFE0000000000
//#define A_FB        0xE001000000000000
//#define A_IN        0x0000020000000000
//#define A_OUT       0x8000000000000000
//#define A_CLK       0x0008000000000000
//
//#define B_MASK      0x000001FFFFF10000
//#define B_FB        0x0000018000000000
//#define B_IN        0x0000000000010000
//#define B_OUT       0x0000010000000000
//#define B_CLK       0x0000000020000000
//
//#define C_MASK      0x000000000007FFFF
//#define C_FB        0x0000000000070080
//#define C_IN        0x0000000000000001
//#define C_OUT       0x0000000000040000
//#define C_CLK       0x0000000000000400
//
//#define ACTIVE_MASK 0xFFF0000000000000
//
////#define ALL_OUT     (A_OUT | B_OUT | C_OUT)
//#define ALL_OUT     0x8000010000040000
//#define AB_CLK      0x0008000020000000
//#define BC_CLK      0x0000000020000400
//#define AC_CLK      0x0008000000000400
//// ----------
//
//
//// XXX not used? 
//// ---------- shifting LFSRs
//inline ulong shiftLFSRs(ulong reg) {
//    // shift all
//    ulong reg_new = reg << 1;
//    // mask feedback bits and compute xor 
//    uint fbA = BITXOR(reg & A_FB);
//    uint fbB = BITXOR(reg & B_FB);
//    uint fbC = BITXOR(reg & C_FB);
//    // apply feedback
//    reg_new = fbA ? reg_new | A_IN : reg_new & ~A_IN;
//    reg_new = fbB ? reg_new | B_IN : reg_new & ~B_IN;
//    reg_new = fbC ? reg_new | C_IN : reg_new & ~C_IN;
//    // clock bits
//    uint clkAB = BITXOR(reg & AB_CLK);
//    uint clkBC = BITXOR(reg & BC_CLK);
//    uint clkAC = BITXOR(reg & AC_CLK);
//    // majority rule
//    uint clockA = ~(clkAB & clkAC);
//    uint clockB = ~(clkAB & clkBC);
//    uint clockC = ~(clkBC & clkAC);
//    // mask out clocked ones
//    ulong mask = (clockA? A_MASK : 0) | (clockB? B_MASK : 0) | (clockC? C_MASK : 0);
//    return (reg_new & mask) | (reg & ~mask);
//}
//// ----------
