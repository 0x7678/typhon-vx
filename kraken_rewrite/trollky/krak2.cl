
#define BSIZE (get_local_size(0)*get_local_size(1)*get_local_size(2))
#define BCOUNT (get_num_groups(0)*get_num_groups(1)*get_num_groups(2))
#define LID (get_local_id(0) + get_local_size(0)*(get_local_id(1) + get_local_size(1)*get_local_id(2)))
#define BID (get_group_id(0) + get_num_groups(0)*(get_group_id(1) + get_num_groups(1)*get_group_id(2))
#define GID (LID + (BID * BSIZE))

/* ================================================================================================== */
/*        using "async_work_group_copy".  This version is optimized for the ACCELERATOR device        */
/* ================================================================================================== */

/* =========================================================== */
/* Grab a pile of input  data into local storage         */
/* =========================================================== */
#define GET_INPUT(_block_size, _block_id) {                                         \
   event_p = async_work_group_copy((local ulong *) &state_l[0],                     \
                                             (global ulong *) &state_g[_block_id*_block_size],  \
                                             (size_t) (_block_size),                          \
                                             (event_t) 0);                                    \
}


kernel void krak(global ulong[] state_g, uint statesCount, uint rounds, uint ctrl) {
    //assync copy to local area - useful for ATI?
    //local ulong state_l [BSIZE];
    //private event_t event_p = async_work_group_copy((local ulong *) &state_l[0],  (global ulong *) &state_g[BID*BSIZE],  (size_t) (BSIZE),  (event_t) 0);   
    //GET_INPUT(BSIZE,BID)

    // my ID - should be GID? why *2?
    private size_t my = GID*2; 
    //private size_t my = LID*2

    // LFSRs state (A - 19bit, B - 22bit, C - 23bit)
    // AAAAAAAA AAAAAAAA AAABBBBB BBBBBBBB BBBBBBBB BCCCCCCC CCCCCCCC CCCCCCCC
    // A        0xFFFFE00000000000
    // fbAmask  0xE400000000000000
    // fbAbit   0x0000200000000000
    // outAbit  0x8000000000000000
    // clkA     0x0020000000000000
    // B        0x00001FFFFF800000
    // fbBmask  0x0000180000000000
    // fbBbit   0x0000000000800000
    // outBbit  0x0000100000000000
    // clkC     0x0000000200000000
    // C        0x00000000007FFFFF
    // fbCmask  0x0000000000700080
    // fbCbit   0x0000000000000001
    // outCbit  0x0000000000400000
    // clkC     0x0000000000000400
    // outbits  0x8000100000400000
    private ulong state, state_out;
    private ulong reg,reg_new;
    private ulong ifbA,ifbB,ifbC;
    private ulong mask;
    private uint clkA,clkB,clkC,clockA,clockB,clockC;
    private uint result;
    private uint active,inactive;
    
    // copy data 
    if (my < statesCount) {
        state = state_g[my+1]; // = state_l[my];
        state_out = state_g[my]; // = state_l[my+1]

        for (uint i = 0; i < rounds; i++) {
            // set LFSRs internal state
            reg = state ^ state_out;

            // detect inactive 
            inactive = ! (reg &  0x000000000007FF000);
#ifdef DP_BIT_12
            inactive &= ! (reg & 0x00000000000000800);
#endif
#ifdef DP_BIT_13
            inactive &= ! (reg & 0x00000000000000400);
#endif
#ifdef DP_BIT_14
            inactive &= ! (reg & 0x00000000000000200);
#endif
            active = ! inactive
            // warm  up;
            for (uint j = 0; j < 99; j++) {
                // inverse feedback
                ifbA = popcount(reg & 0xE400000000000000) & 1;
                ifbB = popcount(reg & 0x0000180000000000) & 1;
                ifbC = popcount(reg & 0x0000000000700080) & 1;
                // clock bits
                clkA = !(reg & 0x0020000000000000);
                clkB = !(reg & 0x0000000200000000);
                clkC = !(reg & 0x0000000000000400);
                // majority rule
                clockA = clkB ^ clkC;
                clockB = clkA ^ clkB;
                clockC = clkA ^ clkC;
                // shift all 
                reg_new = reg << 1;
                // apply feedback
                reg_new = ifbA ? reg_new & ~0x0000200000000000 : reg_new | 0x0000200000000000;
                reg_new = ifbB ? reg_new & ~0x0000000000800000 : reg_new | 0x0000000000800000; 
                reg_new = ifbC ? reg_new & ~0x0000000000000001 : reg_new | 0x0000000000000001;
                // mask out clocked ones
                mask =(clockA? 0xFFFFE00000000000 : 0) | 
                        (clockB? 0x00001FFFFF800000 : 0) | 
                        (clockC? 0x00000000007FFFFF : 0);
                reg = (reg_new & mask) | (reg & ~mask);
            }
            // gen stream of 64 bits
            for (uint j = 1; j > 0; j <<= 1) {
                // inverse feedback
                ifbA = popcount(reg & 0xE400000000000000) & 1;
                ifbB = popcount(reg & 0x0000180000000000) & 1;
                ifbC = popcount(reg & 0x0000000000700080) & 1;
                // clock bits
                clkA = !(reg & 0x0020000000000000);
                clkB = !(reg & 0x0000000200000000);
                clkC = !(reg & 0x0000000000000400);
                // majority rule
                clockA = clkB ^ clkC;
                clockB = clkA ^ clkB;
                clockC = clkA ^ clkC;
                // shift all 
                reg_new = reg << 1;
                // set feedback bits
                reg_new = ifbA ? reg_new & ~0x0000200000000000 : reg_new | 0x0000200000000000;
                reg_new = ifbB ? reg_new & ~0x0000000000800000 : reg_new | 0x0000000000800000; 
                reg_new = ifbC ? reg_new & ~0x0000000000000001 : reg_new | 0x0000000000000001;
                // mask out clocked LFSRs
                mask =(clockA? 0xFFFFE00000000000 : 0) | 
                        (clockB? 0x00001FFFFF800000 : 0) | 
                        (clockC? 0x00000000007FFFFF : 0);
                reg = (reg_new & mask) | (reg & ~mask);
                // result  is xor of LFSRs outputs
                result = ! (popcount(reg & 0x8000100000400000) & 1);
                // update state
                state_out = active ? state_out : (result ? state_out | j : state_out & ~j);
            }
        }
    }
    state_g[my] = state_out;
}
