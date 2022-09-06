
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

size_t array_size = 4;
uint8_t victim_arr[200] = {1, 2, 3, 4};

uint8_t secret = 123;

// purpose of calling gadget: push return address to RSB
// create mismatch btw RSB and software stack
// gadget cleans up the effect of function calls itself by popping
// off the frame that includes the return address
void gadget_variant3()
{
    // TODO: Figure out how to remove the current return address from the stack frame
    // and write assembly code.
    // the 3 pops remove address from stack stopping at the next return address
    __asm__ __volatile__ ("pop %rdi\n\tpop %rdi\n\tpop %rdi\n\tnop");
}

uint8_t victim(size_t idx)
{
    gadget_variant3();
    // TODO: Using probe_array, encode secret to side-channel
    return probe_array[123 * CACHE_BLOCK_SIZE];
}

int main()
{
    m5_set_numflush(1);

    size_t attack_idx = &secret - victim_arr;

    for (int i = 0; i < NUM_CACHE_BLOCKS; i++) {
        _mm_clflush(&probe_array[i * CACHE_BLOCK_SIZE]);
    }
    _mm_mfence();

    victim(attack_idx);

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
}
