#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <x86intrin.h>
#include <gem5/m5ops.h>

#define CACHE_BLOCK_SIZE (64)
#define L1_HIT_THRESOLD (45)

//---------------------------------------------
// TODO:
// Declare probe_array for Flush+Reload Side-channel
// Size of each element is one byte
// Resolution of the side-channel is 256.
// e.g., Side-channel can encode 256 different numbers
//---------------------------------------------
#define NUM_CACHE_BLOCKS (256)
uint8_t probe_array[NUM_CACHE_BLOCKS * CACHE_BLOCK_SIZE];

unsigned long hit_latencies[NUM_CACHE_BLOCKS];
unsigned long miss_latencies[NUM_CACHE_BLOCKS];
int main (int argc, char *argv[])
{
    // Do not erase this line
    m5_set_numflush(1);

    for (int i = 0; i < NUM_CACHE_BLOCKS; i++) {
        // Flush a single cache line
        _mm_clflush(&probe_array[i * CACHE_BLOCK_SIZE]);
    }
    // Wait until all memory operations are complete.
    _mm_mfence();

    for (int i = 0; i < NUM_CACHE_BLOCKS; i++){
        unsigned int junk;
        // Read 64-bit integer tick count.
        unsigned long time1 = __rdtscp(&junk);
        junk ^= probe_array[i * CACHE_BLOCK_SIZE];
        unsigned long time2 = __rdtscp(&junk);
        miss_latencies[i] = time2 - time1;
    }
    
    for (int i = 0; i < NUM_CACHE_BLOCKS; i++) {
        // Write to cache block
        probe_array[i * CACHE_BLOCK_SIZE] = 1;
        uint8_t junk = probe_array[i * CACHE_BLOCK_SIZE];
    }
    // Wait until all memory operations are complete.
    _mm_mfence();

    for (int i = 0; i < NUM_CACHE_BLOCKS; i++){
        unsigned int junk;
        // Read 64-bit integer tick count.
        unsigned long time1 = __rdtscp(&junk);
        junk ^= probe_array[i * CACHE_BLOCK_SIZE];
        unsigned long time2 = __rdtscp(&junk);
        hit_latencies[i] = time2 - time1;
    }

    // Print out miss latency of cache blocks of probe_array
    printf("$L1D miss latencies\n");
    for (int i = 0; i < NUM_CACHE_BLOCKS; i++) {
        printf("%d: %lu\n", i, miss_latencies[i]);
    }
    
    printf("$L1D hit latencies\n");
    for (int i = 0; i < NUM_CACHE_BLOCKS; i++) {
        printf("%d: %lu\n", i, hit_latencies[i]);
    }

    return 0;
}
