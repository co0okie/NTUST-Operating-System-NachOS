#include "syscall.h"
main()
{
    int	n, m;
    for (n=1000;n<1200;n++) {
        for (m=0;m<50;m++);
        PrintInt(n);
    }
}
