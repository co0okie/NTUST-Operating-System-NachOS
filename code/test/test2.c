#include "syscall.h"

main()
{
    int n, m;
    for (n=2000;n<2200;n++) {
        for (m=0;m<50;m++);
        PrintInt(n);
    }
}
