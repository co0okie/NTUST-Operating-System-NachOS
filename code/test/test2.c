#include "syscall.h"

#define arr_size 128 / sizeof(short) * 4
short arr[arr_size];
int i, j;

main() {
    for (i=0; i<arr_size;i++) {
        for (j=0;j<50;j++);
        arr[i] = i;
        PrintInt(20000 + i);
    }
}
