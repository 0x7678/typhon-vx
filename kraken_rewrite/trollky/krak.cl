// ---------- macros to derive linear indices of kernels
#define BLOCK_SIZE (get_local_size(0)*get_local_size(1)*get_local_size(2))
#define BLOCK_COUNT (get_num_groups(0)*get_num_groups(1)*get_num_groups(2))
#define LOCAL_ID (get_local_id(0) + get_local_size(0)*(get_local_id(1) + get_local_size(1)*get_local_id(2)))
#define BLOCK_ID (get_group_id(0) + get_num_groups(0)*(get_group_id(1) + get_num_groups(1)*get_group_id(2)))
#define GLOBAL_ID (get_global_id(0) + get_global_size(0)*(get_global_id(1) + get_global_size(1)*get_global_id(2)))
//#define GLOBAL_ID (LOCAL_ID + (BLOCK_ID * BLOCK_SIZE)) // ugly
// ----------

// ---------- popcount definition
// if we don`t have C1.2 or specific extension - popcout is NOT builtin
#pragma OPENCL EXTENSION cl_amd_popcnt : enable
#if ! (defined CL_VERSION_1_2 || defined cl_amd_popcnt)
#if defined POPCOUNT_NAIVE
uint popcount(private ulong x) {
    //This is a naive implementation, shown for comparison,
    //and to help in understanding the better functions.
    //It uses 24 arithmetic operations (shift, add, and).
    x = (x & 0x5555555555555555 ) + ((x >>  1) & 0x5555555555555555 ); //put count of each  2 bits into those  2 bits 
    x = (x & 0x3333333333333333 ) + ((x >>  2) & 0x3333333333333333 ); //put count of each  4 bits into those  4 bits 
    x = (x & 0x0f0f0f0f0f0f0f0f ) + ((x >>  4) & 0x0f0f0f0f0f0f0f0f ); //put count of each  8 bits into those  8 bits 
    x = (x & 0x00ff00ff00ff00ff ) + ((x >>  8) & 0x00ff00ff00ff00ff ); //put count of each 16 bits into those 16 bits 
    x = (x & 0x0000ffff0000ffff ) + ((x >> 16) & 0x0000ffff0000ffff ); //put count of each 32 bits into those 32 bits 
    x = (x & 0x00000000ffffffff ) + ((x >> 32) & 0x00000000ffffffff ); //put count of each 64 bits into those 64 bits 
    return x;
}
#elif defined POPCOUNT_ITERATED
inline uint popcount(private ulong x) {
    //This is better when most bits in x are 0
    //It uses 3 arithmetic operations and one comparison/branch per "1" bit in x.
    uint count;
    for (count=0; x; count++)
        x &= x-1;
    return count;
}
#elif defined POPCOUNT_FAST
uint popcount(private ulong x) {
    //This uses fewer arithmetic operations than any other known  
    //implementation on machines with fast multiplication.
    //It uses 12 arithmetic operations, one of which is a multiply.
    x -= (x >> 1) & 0x5555555555555555; //put count of each 2 bits into those 2 bits
    x = (x & 0x3333333333333333) + ((x >> 2) & 0x3333333333333333); //put count of each 4 bits into those 4 bits 
    x = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0f; //put count of each 8 bits into those 8 bits 
    return (x * 0x0101010101010101)>>56; //returns left 8 bits of x + (x<<8) + (x<<16) + (x<<24) + ... 
}
#else
inline uint popcount(private ulong x) {
    // this is superfast
    return __builtin_popcount(x);
}
#endif
#endif
// ----------

// ---------- some useful bitstring operations (NOT bitwise)
// !0x0000 -> 1
// !0x0010 -> 0
#define BITNOR(x) (!(x)) // (x == 0)
// !!0x0000 -> !1 -> 0
// !!0x0010 -> !0 -> 1
#define BITOR(x) (!BITNOR(x)) // (x != 0)
// !~0xffff -> !0x0000 -> 1
// !~0x0f00 -> !0xf0ff -> 0
#define BITAND(x) (BITNOR(~(x))) // (~x == 0)
// !!~0xffff -> !!0x0000 -> !1 -> 0
// !!~0x0100 -> !!0xfeff -> !0 -> 1
#define BITNAND(x) (!BITAND(x)) // (~x != 0)
// popcount counts "1" bits so XOR is then LSB of "1" count
// popcount(0x0000) & 1 -> 0x0000 & 1 -> 0
// popcount(0x3010) & 1 -> 0x0003 & 1 -> 1
// popcount(0x3000) & 1 -> 0x0002 & 1 -> 0
#define BITXOR(x) (popcount(x) & 1)
// ---------- 

// ---------- LFSRs state (A - 23bit, B - 22bit, C - 19bit)
// AAAAAAAA AAAAAAAA AAAAAAAB BBBBBBBB BBBBBBBB BBBBBCCC CCCCCCCC CCCCCCCC
// <out------------------in<<out------------------in<<out--------------in<
#define A_MASK      0xFFFFFE0000000000
#define A_FB        0xE001000000000000
#define A_IN        0x0000020000000000
#define A_OUT       0x8000000000000000
#define A_CLK       0x0008000000000000

#define B_MASK      0x000001FFFFF10000
#define B_FB        0x0000018000000000
#define B_IN        0x0000000000010000
#define B_OUT       0x0000010000000000
#define B_CLK       0x0000000020000000

#define C_MASK      0x000000000007FFFF
#define C_FB        0x0000000000070080
#define C_IN        0x0000000000000001
#define C_OUT       0x0000000000040000
#define C_CLK       0x0000000000000400

#define ACTIVE_MASK 0xFFF0000000000000

//#define ALL_OUT     (A_OUT | B_OUT | C_OUT)
#define ALL_OUT     0x8000010000040000
#define AB_CLK      0x0008000020000000
#define BC_CLK      0x0000000020000400
#define AC_CLK      0x0008000000000400
// ----------

// ---------- shifting LFSRs
inline ulong shiftLFSRs(ulong reg) {
    // shift all
    ulong reg_new = reg << 1;
    // mask feedback bits and compute xor 
    uint fbA = BITXOR(reg & A_FB);
    uint fbB = BITXOR(reg & B_FB);
    uint fbC = BITXOR(reg & C_FB);
    // apply feedback
    reg_new = fbA ? reg_new | A_IN : reg_new & ~A_IN;
    reg_new = fbB ? reg_new | B_IN : reg_new & ~B_IN;
    reg_new = fbC ? reg_new | C_IN : reg_new & ~C_IN;
    // clock bits
    uint clkAB = BITXOR(reg & AB_CLK);
    uint clkBC = BITXOR(reg & BC_CLK);
    uint clkAC = BITXOR(reg & AC_CLK);
    // majority rule
    uint clockA = ~(clkAB & clkAC);
    uint clockB = ~(clkAB & clkBC);
    uint clockC = ~(clkBC & clkAC);
    // mask out clocked ones
    ulong mask = (clockA? A_MASK : 0) | (clockB? B_MASK : 0) | (clockC? C_MASK : 0);
    return (reg_new & mask) | (reg & ~mask);
}
// ----------

// ---------- kernel
kernel void krak(global ulong *state_g, uint states, uint rounds, uint ctrl) {
    //assync copy to local area - is it useful for ATI?
    // TODO this cannot work as local definitions must be static and BLOCK_SIZE
    // is here determined in runtime
    ///local ulong state_l[BLOCK_SIZE];
    // TODO not working yet
    /*private event_t ev_copy = async_work_group_copy(
            (local ulong *) state_l,  
            (global ulong *) &state_g[BLOCK_ID*BLOCK_SIZE],  
            (size_t) (BLOCK_SIZE),  
            (event_t) 0);
    wait_group_events(1, &ev_copy);*/
    
    // kernel index
    private size_t my = GLOBAL_ID*2; 
    // local vars
    private ulong state, state_out, reg;
    
    // copy data 
    if (my+1 < states) {
        // more kernels executed than needed
        state = state_g[my+1];
        state_out = state_g[my];
    }

    for (uint i = 0; i < rounds; i++) {
        // set LFSRs internal state
        reg = state ^ state_out;

        // detect inactive
        uint inactive = BITNOR(reg &  ACTIVE_MASK);
        //uint active = ! inactive;
        
        // heat up
        for (uint j = 0; j < 99; j++) {
            reg = shiftLFSRs(reg);
        }
        // gen stream of 64 bits and rewrite existing state
        for (ulong j = A_OUT; j > 0; j >>= 1) {
            reg = shiftLFSRs(reg);
            // xor outputs
            uint result = BITXOR(reg & ALL_OUT);
            // update state
            //state_out = inactive ? state_out : (result ? (state_out | j) : (state_out & ~j));
            state_out = inactive ? state_out : (state_out & ~j);
        }
    }
    // copy data
    if (my+1 < states) {
        state_g[my] = state_out;
        state_g[my+1] = my;
    }
    //assync copy to local area - is it useful for ATI?
    // TODO not working yet
    //barrier(CLK_LOCAL_MEM_FENCE);
    /*ev_copy = async_work_group_copy(
            (global ulong *) &state_g[BLOCK_ID*BLOCK_SIZE],  
            (local ulong *) state_l,
            (size_t) (BLOCK_SIZE),
            (event_t) 0);
    wait_group_events(1, &ev_copy);*/
}
// ----------

