#ifndef SYNTH_H
#define SYNTH_H

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>
#include "spsc_ring_buffer.h"

static char *device = "plughw:0,0";         /* playback device */
static snd_pcm_format_t format = SND_PCM_FORMAT_S16;    /* sample format */
static unsigned int rate = 44100;           /* stream rate */
static unsigned int channels = 1;           /* count of channels */
static unsigned int buffer_time = 500000;       /* ring buffer length in us */
static unsigned int period_time = 100000;       /* period time in us */
static double freq = 440;               /* sinusoidal wave frequency in Hz */
static int verbose = 0;                 /* verbose flag */
static int resample = 1;                /* enable alsa-lib resampling */
static int period_event = 0;                /* produce poll event after each period */
static snd_pcm_t *pcm_handle;
static snd_pcm_hw_params_t *hwparams;
static snd_pcm_sw_params_t *swparams;

static snd_pcm_sframes_t buffer_size;
static snd_pcm_sframes_t period_size;
static snd_output_t *output = NULL;
static int set_hwparams(snd_pcm_t *handle, snd_pcm_hw_params_t *hwparams, snd_pcm_access_t access);
static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams);
static signed short *samples;
static snd_pcm_channel_area_t *areas;
// SPSC lock-free ring buffer
static spsc_ringbuf_t midi_evt_buf;
static void print_snd_pcm_hwparams(snd_pcm_hw_params_t *params);
static void print_snd_pcm_swparams(snd_pcm_sw_params_t *params); 

int init_synth();
typedef enum {
    NOTE_ON,
    NOTE_OFF,
    PITCH_BEND,
    INV
} EventType;

typedef struct {
    EventType type;
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
    uint8_t value;
} MidiEvent;

typedef struct {
    int active;
    int note;
    int velocity;
    int pitch_bend;
    double phase;
} Voice;

bool push_midi_evt(MidiEvent *me);
void *midi_evt_handler(void *arg);
#endif // SYNTH_H
