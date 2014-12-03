#include "A5JobQueue.h"
#include <stdio.h>

A5JobQueue::A5JobQueue(int size) :
    mSize(size),
    mNumWaiting(0)
{
    mInputStart = new deque<uint64_t>[size];
    mInputTarget = new deque<uint64_t>[size];
    mInputRound = new deque<int32_t>[size];
    mInputRoundStop = new deque<int32_t>[size];
    mInputAdvance = new deque<uint32_t>[size];
    mInputContext = new deque<void*>[size];

    /* Init semaphore */
    sem_init( &mMutex, 0, 1 );
}

A5JobQueue::~A5JobQueue()
{
    /* Destroy semaphore */
    sem_destroy(&mMutex);

    delete [] mInputStart;
    delete [] mInputTarget;
    delete [] mInputRound;
    delete [] mInputRoundStop;
    delete [] mInputAdvance;
    delete [] mInputContext;
}


int  A5JobQueue::Submit(uint64_t start_value, uint64_t target,
                   int32_t start_round, int32_t stop_round,
                   uint32_t advance, void* context)
{
    int size = 0;
    sem_wait(&mMutex);

    if (target) {
        /* Keysearches are given priority */
        int qnum = mSize-stop_round;
        mInputStart[qnum].push_front(start_value);
        mInputTarget[qnum].push_front(target);
        mInputRound[qnum].push_front(start_round);
        mInputRoundStop[qnum].push_front(stop_round);
        mInputAdvance[qnum].push_front(advance);
        mInputContext[qnum].push_front(context);
    } else {
        int qnum = start_round;
        mInputStart[qnum].push_back(start_value);
        mInputTarget[qnum].push_back(target);
        mInputRound[qnum].push_back(start_round);
        mInputRoundStop[qnum].push_back(stop_round);
        mInputAdvance[qnum].push_back(advance);
        mInputContext[qnum].push_back(context);
    }
    mNumWaiting++;
    sem_post(&mMutex);
    return mNumWaiting;
}

bool A5JobQueue::PopRequest(A5Il::JobPiece_s* job)
{
    bool found = false;

    /* Get input */
    sem_wait(&mMutex);
    for (int i = 0; i<mSize; i++) {
        if (mInputStart[i].size()) {
            job->start_value = mInputStart[i].front();
            mInputStart[i].pop_front();
            job->end_value = 0;
            job->start_round = mInputRound[i].front();
            mInputRound[i].pop_front();
            job->end_round = mInputRoundStop[i].front();
            mInputRoundStop[i].pop_front();
            job->advance = mInputAdvance[i].front();
            mInputAdvance[i].pop_front();
            job->key_search = mInputTarget[i].front();
            mInputTarget[i].pop_front();
            job->key_found = 0;
            job->context =  mInputContext[i].front();
            mInputContext[i].pop_front();
            job->idle = false;
            job->cycles = 327680 - (job->end_round-job->start_round)*5000-6000;
            found = true;
            mNumWaiting--;
            break;
        }
    }
    sem_post(&mMutex);

    return found;
}
