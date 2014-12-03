/***************************************************************
 * A5/1 Chain generator.
 *
 * Copyright 2010. Frank A. Stevenson. All rights reserved.
 *
 *******************************************************************/

#ifndef A5_IL_PAIR_H
#define A5_IL_PAIR_H

#include "A5Il.h"
#include "CalDevice.h"
#include "CalModule.h"
#include "calcl.h"
#include "cal_ext_counter.h"
#include <sys/time.h>
#include <stdint.h>
#include <map>
#include "A5JobQueue.h"

typedef struct {
    uint32_t val_high;
    uint32_t val_low;
    uint32_t curr_round;
    uint32_t stop_round;
    uint32_t search_high;
    uint32_t search_low;
    uint32_t found_high;
    uint32_t found_low;
} state_t;


class A5Il;

class A5IlPair {
public:
    A5IlPair(A5Il* cont, int num, int dp, int rounds, A5JobQueue* q);
    ~A5IlPair();

    void test();

    bool tick();
    void flush();

    int getNumSlots() { return mNum; }

    int getNumJobs();

    static int getNumDevices();

    void setLimit(int l) {mJobLimit=l;}

private:
    void process();
    void populate();

    int mDevNo;
    int mNum;
    int mNumRounds;

    CalDevice* mDev;
    CalModule* mModule;
    CalModule* mModuleDouble;
    CalModule* mModuleSingle;

    CALresource* mResState;
    state_t*  mState;
    CALresource* mResRoundFunc; 
    CALresource* mResAttention;
    CALresource* mResAttentionSingle;
    uint32_t* mAttention;

    CALobject mObjectDouble;
    CALobject mObjectSingle;
    CALimage  mImageDouble;
    CALimage  mImageSingle;

    A5Il::JobPiece_s* mJobs;
    int mFree;       /* head of free list */
    int mNumJobs;    /* Active jobs       */

    int           mIterCount;
    int           mDp;

    /* Controller */
    A5Il*         mController;

    bool          mWaitState;
    CALevent      mEvent;
    CALcontext*   mCtx;
    CALcounter    mCounter;
    int           mTicks;
    struct timeval mTvStarted;

    int            mMaxCycles;

    int mFreeRound;
    std::map<int,int> mRoundOffsets;
    int getRoundOffset(int adv);

    bool mSingleMode;
    void CheckSize(CALresource* attention);

    A5JobQueue* mJobQueue;
    int mJobLimit;
};

#endif
