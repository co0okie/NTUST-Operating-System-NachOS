#include "syscall.h"
main()
{
    int	n, m;
    for (n=3000;n<3200;n++) {
        for (m=0;m<50;m++);
        PrintInt(n);
    }
}
