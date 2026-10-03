#include <math.h>
#include <stdio.h>

int main(void)
{
    union {
        float f;
        int i;
    } fVal = {0};
    fVal.f = sin(M_PI * .25);
    printf("union_float: %f\nunion_int:   %d\n", fVal.f, fVal.i);
}
