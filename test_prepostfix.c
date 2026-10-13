#include <stdio.h>

int main(void) {
    printf("Hello\n");
    int i = 0;
    if (++i == 1) {
        printf("prefix works in condition\n");
    } else {
        printf("prefix does not work in condition\n");
    }
    return 0;
}
