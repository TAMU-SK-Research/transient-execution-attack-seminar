#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <x86intrin.h>
#include <gem5/m5ops.h>

// L1D = 64kb, assoc = 2, cacheline_size = 64
// uint8_t data[32 * 1024 * 64];
uint8_t data[64 * 32768];

int main(int argc, char* argv[])
{

    //int x; // esp -= 4
    //int y = x; // esp -= 4; mov +8(esp), eax; mov eax, +4(esp)
    //x = 1; // mov 1, &x

    if (argc != 4) {
          printf("usage: %s num_of_flush base_flag\n", argv[0]);
          return -1;
    }

    int num_of_flush = atoi(argv[1]);
    int base_flag = atoi(argv[2]);
    // uint8_t data[num_of_flush * 64];

    if (base_flag == 0) {
        m5_set_numflush(num_of_flush);
        _mm_clflush(&data[0 * 64]);
    }
    else {
        m5_set_numflush(1);
        for (int i = 0; i < num_of_flush; i++)
            _mm_clflush(&data[i * 64]);
    }
    _mm_mfence();

    for (int i = 0; i < num_of_flush; i++) data[i * 64] = 1; // test M
    _mm_mfence();

    unsigned int junk;
    unsigned long time = __rdtscp(&junk);
    if (base_flag == 0) {
        m5_set_numflush(num_of_flush);
        _mm_clflush(&data[0 * 64]);
    }
    else {
        m5_set_numflush(1);
        for (int i = 0; i < num_of_flush; i++)
            _mm_clflush(&data[i * 64]);
    }
    _mm_mfence();
    printf("flush time: %llu\n", __rdtscp(&junk) - time);

    return 0;

}
