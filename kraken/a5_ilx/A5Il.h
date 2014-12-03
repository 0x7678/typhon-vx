/***************************************************************
 * A5/1 Chain generator.
 *
 * Copyright 2010 Frank A. Stevenson. All rights reserved.
 *
 *
 *******************************************************************/

#ifndef A5_IL
#define A5_IL

/* DLL export incatantion */
#if defined _WIN32 || defined __CYGWIN__
#  ifdef BUILDING_DLL
#    ifdef __GNUC__
#      define DLL_PUBLIC __attribute__((dllexport))
#    else
#      define DLL_PUBLIC __declspec(dllexport)
#    endif
#  else
#    ifdef __GNUC__
#      define DLL_PUBLIC __attribute__((dllimport))
#    else
#      define DLL_PUBLIC __declspec(dllimport)
#    endif
#    define DLL_LOCAL
#  endif
#else
#  if __GNUC__ >= 4
#    define DLL_PUBLIC __attribute__ ((visibility("default")))
#    define DLL_LOCAL  __attribute__ ((visibility("hidden")))
#  else
#    define DLL_PUBLIC
#    define DLL_LOCAL
#  endif
#endif


#include <queue>
#include <semaphore.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <map>
#include <deque>

class A5IlPair;
class A5JobQueue;

using namespace std;

class DLL_LOCAL A5Il {
public:
    A5Il(int max_rounds, int condition, uint32_t mask);
    ~A5Il();

    typedef struct {
        uint64_t start_value;
        uint64_t end_value;
        int start_round;
        int end_round;
        int advance;
        uint64_t key_search;
        uint64_t key_found;
        void* context;
        int next_free;      /* for free list housekeeping */
        unsigned int current_round;
        unsigned int cycles;
        bool idle;
    } JobPiece_s; 

    int  Submit(uint64_t start_value, uint64_t target_value,
                int32_t start_round, int32_t stop_round,
                uint32_t advance, void* context);

    bool PopResult(uint64_t& start_value, uint64_t& stop_value,
                   int32_t& start_round, void** context);

    static uint64_t ReverseBits(uint64_t r);

private:
    friend class A5IlPair;

    void PushResult(JobPiece_s*);

    void Process(void);

    int mNumThreads;
    pthread_t* mThreads;
    static void* thread_stub(void* arg);
    static A5Il* mSpawner;

    unsigned int mCondition;
    unsigned int mMaxRound;
    uint32_t mGpuMask;

    bool mIsUsingTables;
    ushort mClockMask[16*16*16];
    unsigned char mTable6bit[1024];
    unsigned char mTable5bit[512];
    unsigned char mTable4bit[256];

    bool mRunning; /* false stops worker thread */

    int mNumSlices;
    A5IlPair** mSlices;
    bool mReady;

    /* Mutex semaphore to protect the queues */
    sem_t mMutex;
    /* Output queues */
    queue< pair<uint64_t,uint64_t> > mOutput;
    queue<int32_t>  mOutputStartRound;
    queue<void*>    mOutputContext;

    /* Input QueueObject */
    A5JobQueue* mJobQueue;
};

#endif
