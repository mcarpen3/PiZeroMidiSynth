#ifndef SYNTH_RECORDER_H
#define SYNTH_RECORDER_H
#include <stdatomic.h>
#include <stdbool.h>

enum gpio_profile_type {
    GPIO_PROFILE_INPUT,
    GPIO_PROFILE_OUTPUT,
};

typedef struct gpio_profile_ {
    enum gpio_profile_type type;
    int num;
    int val;
    int fd;
} gpio_profile; 

volatile sig_atomic_t stop = 0;
atomic_bool record;
atomic_bool recording;
bool previous_recording_state;
void synth_gpio_init();
int verbose = 1;

#endif // SYNTH_RECORDER_H
