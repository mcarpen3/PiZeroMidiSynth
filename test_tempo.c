#include <stdio.h>
#include "synth.h"
#include <math.h>

int main(void) {
    Tempo t = {0};
    double sample = 0.;
    tempo_step   = 1 / tempo_time_secs / rate;
    for (int i = 0; i < rate * 4; i++) {
        sample += tan(t.phase);
        t.phase += tempo_step;
        if (sample > 1) {
            printf("%d\n", i);
            break;
        }
    }

    printf("Hello, world\n");
}
