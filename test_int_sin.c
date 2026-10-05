#include <math.h>
#include <stdio.h>

int main(void) {
    double max_phase = M_PI;
    int rate = 44100;
    int period = 100000;
    int ns = 1000000;
    int period_size = rate * period / ns;
    double freq = 440.;
    double step = max_phase * freq / rate;
    double phase = 0;
    int res;
    unsigned short msb, lsb;
    unsigned int max_sample_val = (1 << (16 - 1)) - 1;
    printf("max_sample_val = %u\n", max_sample_val);
    return 0;
    while (phase < max_phase) {
        res = sin(phase) * max_phase;
        lsb = res & 0xff;
        msb = (res >> 8) & 0xff;
        printf("res = %d, msb = %d, lsb = %d, phase = %f\n",
                res, msb, lsb, phase);
        phase += step;
    }
    return 0;
}


