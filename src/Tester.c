#include <stdio.h>


/* start with nothing */
int main()
{
    const int HIGH_SPEED = (1 << 7);

/* modem is running fast */
/* we are using a hardwired connection */

    const int DIRECT_CONNECT = (1 << 8);
    char flags = 0;

    flags |= HIGH_SPEED;
    /* we are running fast */
    flags |= DIRECT_CONNECT; /* because we are wired together */
    if ((flags & HIGH_SPEED) != 0)
        printf("High speed set\n");
    
    printf("%b", (flags & DIRECT_CONNECT));
    
    if ((flags & DIRECT_CONNECT) != 0)
        printf("Direct connect set\n");

    return 0;
}