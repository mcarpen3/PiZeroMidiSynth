#ifndef SYNTH_RECORDER_H
#define SYNTH_RECORDER_H
#include <stdatomic.h>
#include <stdbool.h>
#include <signal.h>

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

atomic_bool synth_gpio_record_pressed;
atomic_bool synth_gpio_recording;
atomic_bool synth_gpio_join;
bool previous_recording_state;
void synth_gpio_init();
void *handle_gpio_events(void *arg); // thread
double long_press_dur_secs;

#endif // SYNTH_RECORDER_H
