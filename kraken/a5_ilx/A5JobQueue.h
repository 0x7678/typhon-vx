#ifndef A5JobQueue_h
#define A5JobQueue_h

#include <stdint.h>
#include "A5Il.h"
#include <semaphore.h>

class  A5JobQueue {
public:
    A5JobQueue(int size);
    ~A5JobQueue();

    bool PopRequest(A5Il::JobPiece_s*);

    int  Submit(uint64_t start_value, uint64_t target_value,
                int32_t start_round, int32_t stop_round,
                uint32_t advance, void* context);

    int  getNumWaiting() {return mNumWaiting;}

private:

    /* Mutex semaphore to protect the queues */
    sem_t mMutex;
    deque<uint64_t>* mInputStart;
    deque<uint64_t>* mInputTarget;
    deque<int32_t>*  mInputRound;
    deque<int32_t>*  mInputRoundStop;
    deque<uint32_t>* mInputAdvance;
    deque<void*>*    mInputContext;

    int mNumWaiting;
    int mSize;
};






#endif
