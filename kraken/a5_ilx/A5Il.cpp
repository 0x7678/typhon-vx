/***************************************************************
 * A5/1 Chain generator.
 *
 * Copyright 2010. Frank A. Stevenson. All rights reserved.
 *
 *
 *******************************************************************/

#include "A5Il.h"
#include <stdio.h>
#include <sys/time.h>
#include <assert.h>
#include "A5IlPair.h"

using namespace std;

/**
 * Construct an instance of A5 Il searcher
 * create tables and streams
 */
A5Il::A5Il(int max_rounds, int condition, uint32_t mask) :
    mCondition(condition),
    mMaxRound(max_rounds),
    mGpuMask(mask),
    mSlices(NULL),
    mReady(false)
{
mGpuMask = mask; //XXX
printf("ILX GPU mask: %x\n", mGpuMask);
    assert(mCondition==12);

    /* Init semaphore */
    sem_init( &mMutex, 0, 1 );

    /* Make job queue */
    mJobQueue = new A5JobQueue(8);

    /* Start worker thread */
    mRunning = true;
    mNumThreads = 1;
    mThreads = new pthread_t[mNumThreads];
    for (int i=0; i<mNumThreads; i++) {
        pthread_create(&mThreads[i], NULL, thread_stub, (void*)this);
    }

    while(!mReady) {
        usleep(1000);
    }
}

void* A5Il::thread_stub(void* arg)
{
    if (arg) {
        A5Il* a5 = (A5Il*)arg;
        a5->Process();
    }
    return NULL;
}

/**
 * Destroy an instance of A5 Il searcher
 * delete tables and streams
 */
A5Il::~A5Il()
{
    /* stop worker thread */
    mRunning = false;
    for (int i=0; i<mNumThreads; i++) {
        pthread_join(mThreads[i], NULL);
    }

    delete [] mThreads;

    delete mJobQueue;

    sem_destroy(&mMutex);
}
  
int  A5Il::Submit(uint64_t start_value, uint64_t target,
                   int32_t start_round, int32_t stop_round,
                   uint32_t advance, void* context)
{
    if (start_round>=mMaxRound) return -1;
    if (stop_round<0) stop_round = mMaxRound;


    return mJobQueue->Submit( start_value, target,
                              start_round, stop_round,
                              advance, context );

}
  
bool A5Il::PopResult(uint64_t& start_value, uint64_t& stop_value,
                      int32_t& start_round, void** context)
{
    bool res = false;
    sem_wait(&mMutex);
    if (mOutput.size()>0) {
        res = true;
        pair<uint64_t,uint64_t> res = mOutput.front();
        mOutput.pop();
        start_value = res.first;
        stop_value = res.second;
        start_round = mOutputStartRound.front();
        mOutputStartRound.pop();
        void* ctx = mOutputContext.front();
        mOutputContext.pop();
        if (context) *context = ctx;
    }
    sem_post(&mMutex);
    return res;
}

void A5Il::Process(void)
{
    struct timeval tStart;
    struct timeval tEnd;

    int numCores = A5IlPair::getNumDevices();
    
//    mGpuMask=0xff; //XXX
printf("ILX GPU mask2: %x\n", mGpuMask);
    mNumSlices = 0;
    for(int i=0; i<numCores; i++) {
        if ((1<<i)&mGpuMask) mNumSlices++;
    }
    printf("ILX Running on %i GPUs\n", mNumSlices);
    
    mSlices = new A5IlPair*[mNumSlices];
    
    int core = 0;
    for( int i=0; i<numCores ; i++ ) {
        if ((1<<i)&mGpuMask) {
            printf("ILX Init card #%x\n",i);
            mSlices[core] = new A5IlPair( this, i, mCondition, mMaxRound,
                                          mJobQueue);
            core++;
        }
    }
    mReady = true;

    for(;;) {
        bool newCmd = false;
        int total = mJobQueue->getNumWaiting();
        
        for( int i=0; i<mNumSlices ; i++ ) {
            total += mSlices[i]->getNumJobs();
        }        

        // printf("*");

        if (total) {
            /* do load balancing */
            int even = total / mNumSlices;
            // printf("Doing an even %i\n", even);
            for( int i=0; i<mNumSlices ; i++ ) {
                mSlices[i]->setLimit(even);
                newCmd |= mSlices[i]->tick();
            }
        } else {
            /* Empty pipeline */
            usleep(1000);
        }

        if (!mRunning) break;
    }
    
    for( int i=0; i<mNumSlices ; i++ ) {
        delete mSlices[i];
    }
    delete[] mSlices;
    mSlices = NULL;

}

void A5Il::PushResult(JobPiece_s* job)
{
    /* Report completed chains */
    sem_wait(&mMutex);

    uint64_t res = job->key_found ? job->key_found : job->end_value;
    res = ReverseBits(res);
    mOutput.push( pair<uint64_t,uint64_t>(job->start_value,res) );
    mOutputStartRound.push( job->start_round );
    mOutputContext.push( job->context );

    sem_post(&mMutex);
}



/* Reverse bit order of an unsigned 64 bits int */
uint64_t A5Il::ReverseBits(uint64_t r)
{
    uint64_t r1 = r;
    uint64_t r2 = 0;
    for (int j = 0; j < 64 ; j++ ) {
        r2 = (r2<<1) | (r1 & 0x01);
        r1 = r1 >> 1;
    }
    return r2;
}


/* Stubs for shared library - exported without name mangling */

extern "C" {

static class A5Il* a5Instance = 0;

bool DLL_PUBLIC A5IlInit(int max_rounds, int condition, uint32_t mask)
{
    if (a5Instance) return false;
    a5Instance = new A5Il(max_rounds, condition, mask);
    return true;
}

int  DLL_PUBLIC A5IlSubmit(uint64_t start_value, int32_t start_round,
                           uint32_t advance, void* context)
{
    if (a5Instance) {
        return a5Instance->Submit(start_value, 0ULL, start_round, -1,
                                  advance, context);
    }
    return -1; /* Error */
}

int  DLL_PUBLIC A5IlKeySearch(uint64_t start_value, uint64_t target,
                              int32_t start_round, int32_t stop_round,
                              uint32_t advance, void* context)
{
    if (a5Instance) {
        return a5Instance->Submit(start_value, target, start_round,
                                  stop_round, advance, context);
    }
    return -1; /* Error */
}


bool DLL_PUBLIC A5IlPopResult(uint64_t& start_value, uint64_t& stop_value,
                               int32_t& start_round, void** context)
{
    if (a5Instance) {
        return a5Instance->PopResult(start_value, stop_value, start_round,
                                     context);
    }
    return false; /* Nothing popped */ 
}

void DLL_PUBLIC A5IlShutdown()
{
    delete a5Instance;
    a5Instance = NULL;
}

}
