#include "syscall.h"
#define arr_size 128 / sizeof(short) * 8
short arr[arr_size];
int i, j;

main() {
    for (i = 0; i < arr_size; i++) {
        arr[i] = i;
        PrintInt(30000 + arr[i]);
    }
}
