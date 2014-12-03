#include <stdio.h>
#include "Fragment.h"
#include "Kraken.h"
#include "A5IlStubs.h"
//#include "../a5_cpu/A5CpuStubs.h"
#include "../a5_ati/A5AtiStubs.h"

static uint64_t kr02_whitening(uint64_t key)
{
    int i;
    uint64_t white = 0;
    uint64_t bits = 0x93cbc4077efddc15ULL;
    uint64_t b = 0x1;
    while (b) {
        if (b & key) {
            white ^= bits;
        }
        bits = (bits<<1)|(bits>>63);
        b = b << 1;
    }
    return white;
}

static uint64_t kr02_mergebits(uint64_t key)
{
    uint64_t r = 0ULL;
    uint64_t b = 1ULL;
    unsigned int i;

    for(i=0;i<64;i++) {
        if (key&b) {
            r |= 1ULL << (((i<<1)&0x3e)|(i>>5));
        }
        b = b << 1;
    }
    return r;
}

void ApplyIndexFunc(uint64_t& start_index, int bits)
{
    uint64_t w = kr02_whitening(start_index);
    start_index = kr02_mergebits((w<<bits)|start_index);
}

Fragment::Fragment(uint64_t plaintext, unsigned int round,
                   DeltaLookup* table, unsigned int advance) :
    mKnownPlaintext(plaintext),
    mNumRound(round),
    mAdvance(advance),
    mTable(table),
    mState(0),
    mJobNum(0),
    mClientId(0),
    mEndpoint(0),
    mBlockStart(0),
    mStartIndex(0)
{
}

void Fragment::processBlock(const void* pDataBlock) // z NCQ device přišel přečtený blok (pDataBlock je dlouhý 4096), musíme ho zpracovat
{
//X    printf("Searching for endpoint, datablock beginning %llx, blockstart %d, endpoint %d, startindex %d\n",pDataBlock, mBlockStart, mEndpoint, mStartIndex);
    // blok se prohledá a zkusí se v něm najít začátek chainu
    int res = mTable->CompleteEndpointSearch(pDataBlock, mBlockStart,
                                             mEndpoint, mStartIndex);
/*
pDataBlock - 4KiB blok z tabulky, ve kterém by se podle indexu mělo nacházet to, co hledáme
mBlockStart - hodnota, od které se inkrementuje delta
mEndpoint - distinguished point, který jsme spočítali
mStartIndex - tam se uloží výsledek - stav na začátku chainu (pokud v tabulce je)
*/


    if (res) {
        /* Found endpoint */
//X        printf("Found endpoint: %llx %llx\n", mEndpoint, mStartIndex);
	// mStartIndex je začátek chainu, musíme dopočítat zbytek
        // Co je mEndpoint? Nejpíš to, co hledáme (má na konci nuly)
        uint64_t search_rev = mStartIndex;
        // whitening - nějaké přexorování a bitshift, nevím, proč se to dělá
        // http://en.wikipedia.org/wiki/Block_cipher#Iterated_block_ciphers
        ApplyIndexFunc(search_rev, 34);
        if (Kraken::getInstance()->isUsingAti()) {
//X            printf("NumRound is %d\n", mNumRound);
            if (mNumRound) {
//X		printf("GPU submitting %llx, NumRound %d, advance %d\n",search_rev, mNumRound, mAdvance);
		// Advance je typ přechodové funkce použité při generování chainu,
                //    je shodný se jménem souboru s tabulkou/indexem (čísla 100-500)
                // mNumRound je barva
                // uint64_t start_value, unsigned int stop_round, uint32_t advance, void* context
                int res = A5AtiSubmitPartial(search_rev, mNumRound, mAdvance, this);

/*
Kraken dělá Submit, když hledá endpoint. My děláme SubmitPartial, protože dopočítáváme chain odzačátku

int  AtiA5::Submit       (uint64_t start_value, unsigned int start_round, uint32_t advance, void* context)
int  AtiA5::SubmitPartial(uint64_t start_value, unsigned int stop_round,  uint32_t advance, void* context) ←←pointer na nás

*/
                if (res<0) printf("Fail\n");
                mState = 3;
            } else {
                // XXX WTF Proběhne to ve 27 % případů než větev nahoře s A5AtiSubmitPartial 
//X		printf("CPU submitting %llx, mKnownPlaintext %d, NumRound+1 %d, advance %d\n",search_rev, mKnownPlaintext,mNumRound+1, mAdvance);
                // uint64_t start_value, uint64_t target, int32_t start_round, int32_t stop_round, uint32_t advance, void* context
                A5CpuKeySearch(search_rev, mKnownPlaintext, 0, mNumRound+1, mAdvance, this);
/*

CPU: uint64_t start_value, uint64_t target, int32_t start_round,     int32_t stop_round, uint32_t advance, void* context
GPU: uint64_t start_value,                                      unsigned int stop_round, uint32_t advance, void* context

*/

                mState = 2;
            }
        } else {
//            printf("GPU support not enabled or something went terribly wrong!\n");
            A5CpuKeySearch(search_rev, mKnownPlaintext, 0, mNumRound+1, mAdvance, this);
            mState = 2;
        }
    } else {
        /* not found */
//X        printf("Not found endpoint: %llx\n", mEndpoint);
        return Kraken::getInstance()->removeFragment(this);
    }
}

void Fragment::handleSearchResult(uint64_t result, int start_round)
{
    // printf("handle %llx %i %i\n", result, start_round, mState);
    // printf("P");
    //fflush(stdout);
    if (mState==0) {
        mEndpoint = result;
        mTable->StartEndpointSearch(this, mEndpoint, mBlockStart);
        mState = 1;
        if (mBlockStart==0ULL) {
            /* Endpoint out of range */
            return Kraken::getInstance()->removeFragment(this);
        }
    } else if (mState==2) {
        if (start_round<0) {
            /* Found */
            char msg[128];
            snprintf(msg,128,"Found %016llx @ %i  #%i  (table:%i)\n", result, mBitPos, mJobNum, mAdvance);
            printf("%s",msg);
            Kraken::getInstance()->reportFind(string(msg),mClientId);
        }
        return Kraken::getInstance()->removeFragment(this);
    } else {
        /* We are here because of a partial GPU search */
        /* search final round with CPU */

/* Co se tady sakra dopočítává?
 Proběhne to přesně stejně krát, jako A5AtiSubmitPartial

V A5Slice k tomu píšou:
 CPU processes the tricky bits (switching rounds) after the
 GPU has done its part
*/


//X	printf("CPU search for %llx, plaintext %llx, round %d, advance %d\n", result, mKnownPlaintext, mNumRound, mAdvance);
	//uint64_t start_value, uint64_t target, int32_t start_round, int32_t stop_round, uint32_t advance, void* context
        A5CpuKeySearch(result, mKnownPlaintext,          mNumRound-1,         mNumRound+1,        mAdvance,         this);
        mState = 2;
    }
}
