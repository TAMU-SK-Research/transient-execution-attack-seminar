#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <x86intrin.h>
#include <gem5/m5ops.h>

#define CACHE_BLOCK_SIZE (64)

// TODO: Figure out the L1 Hit Latency
#define L1_HIT_THRESOLD (/*FILLME*/)

//---------------------------------------------
// TODO:
// Declare probe_array for Flush+Reload Side-channel
// Size of each element is one byte
// Resolution of the side-channel is 256.
// e.g., Side-channel can encode 256 different numbers
//---------------------------------------------
#define NUM_CACHE_BLOCKS (/*FILLME*/)
uint8_t probe_array[/*FILLME*/];

// An array to store latencies of cache blocks corresponding to probe_array
unsigned long hit_latencies[NUM_CACHE_BLOCKS];
unsigned long miss_latencies[NUM_CACHE_BLOCKS];

int main (int argc, char *argv[])
{
    // Do not erase this line
    m5_set_numflush(1);

    for (int i = 0; i < NUM_CACHE_BLOCKS; i++) {
        // TODO: Flush a single cache block to memory
        // _mm_clush function takes a memory address to flush
        _mm_clflush(/*FILLME*/);
    }
    // Wait until all memory operations are complete.
    _mm_mfence();

    // Measure access latencies of cache blocks of probe_array
    for (int i = 0; i < NUM_CACHE_BLOCKS; i++) {
        uint64_t junk;
        // Read 64-bit integer tick count.
        uint64_t time1 = __rdtscp(&junk);
        // TODO: access to cache block
        junk ^= probe_array[/*FILLME:*/];
        uint64_t time2 = __rdtscp(&junk);
        miss_latencies[i] = time2 - time1;
    }
    
    for (int i = 0; i < NUM_CACHE_BLOCKS; i++) {
        // TODO: Install data of probe_array to L1D Cache
    }
    // Wait until all memory operations are complete.
    _mm_mfence();

    for (int i = 0; i < NUM_CACHE_BLOCKS; i++){
        unsigned int junk;
        // Read 64-bit integer tick count.
        unsigned long time1 = __rdtscp(&junk);
        junk ^= probe_array[/*FILLME*/];
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
