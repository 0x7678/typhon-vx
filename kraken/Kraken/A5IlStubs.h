#ifndef A5_IL_STUBS
#define A5_IL_STUBS

#if 0
#include "../a5_cpu/A5CpuStubs.h"
#else

#include <stdint.h>

bool A5IlInit(int max_rounds, int condition, uint32_t gpumask);
int  A5IlSubmit(uint64_t start_value, int32_t start_round,
                 uint32_t advance, void* context);
int  A5IlKeySearch(uint64_t start_value, uint64_t target,
                    int32_t start_round, int32_t stop_round,
                    uint32_t advance, void* context);
bool A5IlPopResult(uint64_t& start_value, uint64_t& stop_value,
                    int32_t& start_round, void** context);  
void A5IlShutdown();


#define A5CpuInit(x,y,z) A5IlInit((x),(y),(z))
#define A5CpuSubmit A5IlSubmit
#define A5CpuKeySearch A5IlKeySearch
#define A5CpuPopResult A5IlPopResult
#define A5CpuPopResult A5IlPopResult
#define A5CpuShutdown  A5IlShutdown
#endif
#endif
