#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <x86intrin.h>
#include <gem5/m5ops.h>

// L1D = 64kb, assoc = 2, cacheline_size = 64
//uint8_t data[32 * 1024 * 64];
//pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

uint8_t data[64 * 32768];

struct args
{
    //uint8_t* d;
    int flush_num;
};

void *func(void *input)
{
    //pthread_mutex_lock(&mutex1);
    unsigned int junk1;
    for (int i = 0; i < ((struct args*)input)->flush_num; i++)
        junk1 ^= data[i * 64];
    //pthread_mutex_unlock(&mutex1);
}

int main(int argc, char** arv)
{

    //int x; // esp -= 4
    //int y = x; // esp -= 4; mov +8(esp), eax; mov eax, +4(esp)
    //x = 1; // mov 1, &x

    if (argc != 4) {
          printf("usage: %s num_of_flush base_flag num_of_threads\n", arv[0]);
          return -1;
    }

    int NTHREADS = atoi(arv[3]);
    int num_of_flush = atoi(arv[1]);
    int base_flag = atoi(arv[2]);
    //uint8_t data[num_of_flush * 64];

    struct args *input = (struct args *)malloc(sizeof(struct args));
    // input->d = data;
    input->flush_num = num_of_flush;

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

    unsigned int junk1;
    for (int i = 0; i < num_of_flush; i++) junk1 ^= data[i * 64];
    _mm_mfence();

    pthread_t thread_id[NTHREADS];
    for (int i = 0; i < NTHREADS; i++)
        pthread_create(&thread_id[i], NULL, func, (void *)input);

    for (int j = 0; j < NTHREADS; j++)
        pthread_join(thread_id[j], NULL);

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
