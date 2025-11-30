#include "syscall.h"

int	n = 10, m = 20;

main()
{
    for (n=3000;n<3200;n++) {
        for (m=0;m<50;m++);
        PrintInt(n);
    }
}
