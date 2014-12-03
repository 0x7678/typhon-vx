/***************************************************************
 * A5/1 Chain generator.
 *
 * Copyright 2010. Frank A. Stevenson. All rights reserved.
 *
 *******************************************************************/

#include "A5IlPair.h"
#include <assert.h>
#include <sys/time.h>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include "Advance.h"

#include "kernelLib.h"

#define MAX_RFTABLE_ENTRIES 1024

#define DISASSEMBLE 0

#if DISASSEMBLE
static FILE* gDisFile;

static void logger(const char*msg)
{
    if (gDisFile) {
        fwrite(msg,strlen(msg),1,gDisFile);
    }
}
#endif


PFNCALCTXCREATECOUNTER Ext_calCtxCreateCounter = 0;
PFNCALCTXBEGINCOUNTER Ext_calCtxBeginCounter = 0;
PFNCALCTXENDCOUNTER Ext_calCtxEndCounter = 0;
PFNCALCTXDESTROYCOUNTER Ext_calCtxDestroyCounter = 0;
PFNCALCTXGETCOUNTER Ext_calCtxGetCounter = 0;

A5IlPair::A5IlPair(A5Il* cont, int dev, int dp, int rounds,A5JobQueue* q) :
    mNumRounds(rounds),
    mController(cont),
    mState(0),
    mDp(dp),
    mWaitState(false),
    mTicks(0),
    mAttention(0),
    mFreeRound(0),
    mSingleMode(false),
    mJobQueue(q)
{
    mDevNo = dev;
    mMaxCycles = 327650;

    // CAL setup
    assert((dev>=0)&&(dev<CalDevice::getNumDevices()));
    mDev = CalDevice::createDevice(dev);
    assert(mDev);
    mNum = mDev->getDeviceAttribs()->wavefrontSize *
        mDev->getDeviceAttribs()->numberOfSIMD;

    printf("Num threads %i\n", mNum );

    mResState = mDev->resAllocLocal1D(4*mNum, CAL_FORMAT_UINT_4, 
                                      CAL_RESALLOC_CACHEABLE|
                                      CAL_RESALLOC_GLOBAL_BUFFER);
    mResRoundFunc = mDev->resAllocLocal1D(MAX_RFTABLE_ENTRIES,
                                          CAL_FORMAT_UINT_2, 0);
    mResAttention = mDev->resAllocLocal1D(mNum, CAL_FORMAT_UINT_2,
                                          CAL_RESALLOC_CACHEABLE);
    mResAttentionSingle = mDev->resAllocLocal1D(mNum, CAL_FORMAT_UINT_1,
                                          CAL_RESALLOC_CACHEABLE);

    /* Lazy check to ensure that memory has been allocated */
    assert(mResState);
    assert(mResRoundFunc);
    assert(mResAttention);
    assert(mResAttentionSingle);

    /**
     * Load Compile and Link kernels
     */
    unsigned char* kernel = getKernel(0);
    assert(kernel);
    if (calclCompile(&mObjectDouble, CAL_LANGUAGE_IL, (const CALchar*)kernel,
                     mDev->getDeviceInfo()->target) != CAL_RESULT_OK) {
        assert(!"Compilation failed.");
    }
    freeKernel(kernel);

    if (calclLink (&mImageDouble, &mObjectDouble, 1) != CAL_RESULT_OK) {
        assert(!"Link failed.");
    }
#if DISASSEMBLE
    gDisFile = fopen("disassembly_double.txt","w");
    calclDisassembleImage(mImageDouble, logger);
    fclose(gDisFile);
#endif
    mCtx = mDev->getContext();
    mModuleDouble = new CalModule(mCtx);
    if (mModuleDouble==0) {
        assert(!"Could not create module");
    }
    if (!mModuleDouble->Load(mImageDouble)) {
        assert(!"Could not load module image");
    }

    /* repeat for single kernel */
    kernel = getKernel(1);
    assert(kernel);
    if (calclCompile(&mObjectSingle, CAL_LANGUAGE_IL, (const CALchar*)kernel,
                     mDev->getDeviceInfo()->target) != CAL_RESULT_OK) {
        assert(!"Compilation failed.");
    }
    freeKernel(kernel);

    if (calclLink (&mImageSingle, &mObjectSingle, 1) != CAL_RESULT_OK) {
        assert(!"Link failed.");
    }
#if DISASSEMBLE
    gDisFile = fopen("disassembly_single.txt","w");
    calclDisassembleImage(mImageSingle, logger);
    fclose(gDisFile);
#endif
    mCtx = mDev->getContext();
    mModuleSingle = new CalModule(mCtx);
    if (mModuleSingle==0) {
        assert(!"Could not create module");
    }
    if (!mModuleSingle->Load(mImageSingle)) {
        assert(!"Could not load module image");
    }


    unsigned int *dataPtr = NULL;
    CALuint pitch = 0;
    
    if (mSingleMode) {
        mModule = mModuleSingle;
        mModule->Bind("o0",mResAttentionSingle);
    } else {
        mModule = mModuleDouble;
        mModule->Bind("o0",mResAttention);
    }
    mModule->Bind("g[]",mResState);
    mModule->Bind("i0",mResRoundFunc);

    /* Init debug counters */
    if (!Ext_calCtxCreateCounter) {
        calExtGetProc( (CALvoid**)&Ext_calCtxCreateCounter, CAL_EXT_COUNTERS,
                       "calCtxCreateCounter");
    }
    Ext_calCtxCreateCounter( &mCounter, *mCtx, CAL_COUNTER_IDLE );
    if (!Ext_calCtxBeginCounter) {
        calExtGetProc( (CALvoid**)&Ext_calCtxBeginCounter, CAL_EXT_COUNTERS,
                       "calCtxBeginCounter");
    }
    Ext_calCtxBeginCounter( *mCtx, mCounter );
    if (!Ext_calCtxEndCounter) {
        calExtGetProc( (CALvoid**)&Ext_calCtxEndCounter, CAL_EXT_COUNTERS,
                       "calCtxEndCounter");
    }
    if (!Ext_calCtxDestroyCounter) {
        calExtGetProc( (CALvoid**)&Ext_calCtxDestroyCounter, CAL_EXT_COUNTERS,
                       "calCtxDestroyCounter");
    }
    if (!Ext_calCtxGetCounter) {
        calExtGetProc( (CALvoid**)&Ext_calCtxGetCounter, CAL_EXT_COUNTERS,
                       "calCtxGetCounter");
    }

    /* Init free list */
    int jobs = mNum * 2;
    mJobLimit = jobs;
    mJobs = new A5Il::JobPiece_s[jobs];
    for(int i=0; i<jobs; i++) {
        mJobs[i].next_free = i-1;
        mJobs[i].idle = true;
    }
    mFree = jobs - 1;
    mNumJobs = 0;

    populate();
};

A5IlPair::~A5IlPair() {
    delete [] mJobs;

    Ext_calCtxEndCounter(*mCtx,mCounter);
    Ext_calCtxDestroyCounter( *mCtx, mCounter );

    CalDevice::unrefResource(mResState);
    CalDevice::unrefResource(mResRoundFunc);
    CalDevice::unrefResource(mResAttention);
    CalDevice::unrefResource(mResAttentionSingle);

    delete mModuleSingle;
    delete mModuleDouble;
    calclFreeImage(mImageSingle);
    calclFreeImage(mImageDouble);
    calclFreeObject(mObjectSingle);
    calclFreeObject(mObjectDouble);
    delete mDev;

}


int A5IlPair::getRoundOffset(int adv)
{
    std::map<int,int>::iterator it = mRoundOffsets.find(adv);
    if (it!=mRoundOffsets.end())
    {
        return (*it).second;
    }

    assert( (mFreeRound+mNumRounds) <= MAX_RFTABLE_ENTRIES );
        
    Advance* advance = new Advance(adv,mNumRounds);
    unsigned int *dataPtr = NULL;
    CALuint pitch = 0;
    if (calResMap((CALvoid **)&dataPtr, &pitch, *mResRoundFunc, 0)
        != CAL_RESULT_OK) {
        assert(!"Error - now I will go and crash myself.\n");
    }
    const uint32_t* aval = advance->getRFtable();
    for (int j=0; j < mNumRounds ; j++) {
        /* Low high order was swapped in old kernel input*/
        dataPtr[2*(j+mFreeRound)]   = aval[2*j+1];
        dataPtr[2*(j+mFreeRound)+1] = aval[2*j+0];
    }
    calResUnmap(*mResRoundFunc);
    delete advance;
    mRoundOffsets[adv]=mFreeRound;

    int ret = mFreeRound;
    mFreeRound += mNumRounds;
    return ret;
}

/**
 * CPU inserts / removes data after GPU has done its part
 */
void A5IlPair::process()
{
    int entries = mSingleMode ? mNum : (mNum*2);
    for (int i=0; i<entries; i++) {
        A5Il::JobPiece_s* job = &mJobs[i];
        if (!job->idle) {
            // printf("Process %i %i %i-%i\n", i,mAttention[i],
            // mState[i].curr_round,mState[i].stop_round);

            job->cycles -= 1000;
            bool intMerge = job->cycles<0;
            uint32_t att = mAttention[i];
            if (intMerge || att) {
                if (att==2) {
                    /* Key found */
                    job->key_found = ((uint64_t)mState[i].found_high<<32)
                        |mState[i].found_low;
                    // printf("Found state: %08x %08x\n", mState[i].found_high, mState[i].found_low);
                    job->start_round = -1;
                    // printf("Found big fish!!! (%i) %llx %llx\n",i,job->start_value,job->key_search);
                } else {
                    uint64_t res;
                    if (att==0) {
                        /* Internal merges are reported as an invalid
                         * end point */
                        // printf("Killed int merge (%i).\n", i);
                        res = 0xffffffffffffffffULL;
                    } else {
                        /* End of chain found */
                        res = ((uint64_t)mState[i].val_high<<32)|mState[i].val_low;
                        // printf("Found %i %i %i-%i\n", i,att,mState[i].curr_round,
                        //       mState[i].stop_round);
                        // printf("     %i %llx %llx\n",job->start_round,job->start_value, res); 
                    }
                    job->end_value = res;
                }
                mController->PushResult(job);

                /* This item now becomes idle, add to free list */
                /* Send the state straight to state 3 (complete/idle) */
                mState[i].curr_round = 0;
                mState[i].stop_round = 0;
                job->next_free = mFree;
                job->idle = true;
                mFree = i;
                mNumJobs--;
            }
        }
    }

    /* Do load balancing */
    int allowed = mJobLimit-mNumJobs;

    /* Populate to idle queue */
    while( (mFree>=0) && (allowed>0) ) {
        A5Il::JobPiece_s* job = &mJobs[mFree];
        if (mJobQueue->PopRequest(job) ) {
            int i = mFree;
            mFree = job->next_free;
            uint64_t v = A5Il::ReverseBits(job->start_value);
            mState[i].val_high = v >> 32;
            mState[i].val_low  = v;
            v = job->key_search;
            mState[i].search_high = v >> 32;
            mState[i].search_low  = v;
            int off = getRoundOffset(job->advance);
            mState[i].curr_round = off + job->start_round;
            mState[i].stop_round = off + job->end_round;
            job->cycles = (job->end_round-job->start_round + 3)*5000;
            // printf("Insert %i %i %i-%i %llx\n", off, i,mState[i].curr_round,mState[i].stop_round, job->start_value);
            mNumJobs++;
            allowed--;
        } else {
            /* No more jobs */
            break;
        }
    }
}

/* Check size prior to execution  - unmap before returning */
void A5IlPair::CheckSize(CALresource* attention) {
    if (mNumJobs>mNum) {
        calResUnmap(*attention);
        calResUnmap(*mResState);
        mState = 0;
        mAttention = 0;
        if (mSingleMode) {
            /* Switch to double mode */
            printf("Switch to double.\n");
            mModule = mModuleDouble;
            mModule->Bind("o0",mResAttention);
            mModule->Bind("g[]",mResState);
            mModule->Bind("i0",mResRoundFunc);
            mSingleMode = false;
        }
        return;
    }
    /* Ensure all jobs are in lower half */
    int entries = 2*mNum;
    int free = mFree;

    for(int i=mNum;i<entries;i++) {
        if (!mJobs[i].idle) {
            while (mJobs[free].next_free>=mNum) {
                free = mJobs[free].next_free;
            }
            int next_free = mJobs[free].next_free;
            int nn_free = mJobs[next_free].next_free;
            mJobs[next_free] = mJobs[i];
            mState[next_free] = mState[i];
            mJobs[i].idle = true;
            mJobs[free].next_free = i;
            mJobs[i].next_free = nn_free;
            mState[i].curr_round = 0;
            mState[i].stop_round = 0;
            free = i;
        }
    }

    calResUnmap(*attention);
    calResUnmap(*mResState);
    mState = 0;
    mAttention = 0;

    if (!mSingleMode) {
        /* Switch to single mode */
        printf("Switch to single.\n");
        mModule = mModuleSingle;
        mModule->Bind("o0",mResAttentionSingle);
        mModule->Bind("g[]",mResState);
        mModule->Bind("i0",mResRoundFunc);
        mSingleMode = true;
    }
}

/* Initialize the pipeline (set to 0) */
void A5IlPair::populate() {
    // printf("POPULATE !!!");
    unsigned int *dataPtr = NULL;
    CALuint pitch = 0;
    if (calResMap((CALvoid**)&dataPtr, &pitch, *mResState, 0) != CAL_RESULT_OK) 
    {
        assert(!"Can't map mResState resource");
    }
    memset(dataPtr,0,mNum*4*4*sizeof(uint32_t));
    calResUnmap(*mResState);
    if (calResMap((CALvoid**)&dataPtr, &pitch, *mResRoundFunc, 0) != CAL_RESULT_OK) 
    {
        assert(!"Can't map mResRoundFunc resource");
    }
    memset(dataPtr,0,MAX_RFTABLE_ENTRIES*2*sizeof(uint32_t));
    calResUnmap(*mResRoundFunc);    
    mState = 0;
}


bool A5IlPair::tick()
{
    CALuint pitch = 0;

    if(mWaitState) {
        /* Kernel still running */
        if (!mModule->Finished()) return false;
        mWaitState = false;
        struct timeval tv2;
        gettimeofday(&tv2, NULL);
        unsigned long diff = 1000000*(tv2.tv_sec-mTvStarted.tv_sec);
        diff += tv2.tv_usec-mTvStarted.tv_usec;
        // printf("Exec() took %i usec\n",(unsigned int)diff);
    }

    /* Kernel done */
    if (calResMap((CALvoid**)&mState, &pitch, *mResState, 0) != CAL_RESULT_OK) 
    {
        assert(!"Can't map mResState resource");
    }
    CALresource* attention = mSingleMode ? mResAttentionSingle : mResAttention;
    if (calResMap((CALvoid**)&mAttention, &pitch, *attention, 0) != CAL_RESULT_OK) 
    {
        assert(!"Can't map mResState resource");
    }
    process();
    // printf("Active: %d\n", mNumJobs);
    CheckSize(attention);  /* Ensure optimal size (will unmap mem)*/

    if (mNumJobs) {
        CALdomain domain = {0, 0, mNum, 1};
        gettimeofday(&mTvStarted, NULL);
        if (!mModule->Exec(domain)) {
            assert(!"Could not execute module.");
        }
        mModule->Finished();
        calCtxFlush(*mCtx);
        mWaitState = true;
        /* report that something was actually done */
        return true;
    }

#if 0
    /* Ticks that has performed any action make it to here */
    mTicks++;
    if (mTicks%100 == 0) {
        CALfloat perf;
        // Measure performance
        Ext_calCtxEndCounter( *mCtx, mCounter );
        Ext_calCtxGetCounter( &perf, *mCtx, mCounter );
        Ext_calCtxBeginCounter( *mCtx, mCounter );
        printf("Perf %i: %f\n", mDevNo, perf);
    }
#endif


    return false;
}

void A5IlPair::flush()
{
    calCtxFlush(*mCtx);
}

int A5IlPair::getNumJobs()
{
    return mNumJobs;
}


/**
 * Static function to avoid uneccesary dependencies on CalDevice...
 */
int A5IlPair::getNumDevices()
{
    return CalDevice::getNumDevices();
}
