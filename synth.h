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
/**
 * ring buffer length in microsecs
 * e.g. 40,000us = 40ms = .04s
 * @44,100 sample rate, buffer is 44,100 * .04 = 1764 samples/buffer
 */
static unsigned int buffer_time = 40000; // microseconds
/**
 * period time in microsecs
 * e.g. 10,000us = 10ms = 0.01s per period
 * @44,100 sample rate, period is 44,100 * .01 = 441 samples/period
 * Format S16LE means 16 bits per sample, so one period = 16 * 441 / 8 = 882 bytes per period
 */
static unsigned int period_time = 10000; // microseconds
static double freq = 440;               /* sinusoidal wave frequency in Hz */
static int verbose = 0;                 /* verbose flag */
static int resample = 1;                /* enable alsa-lib resampling */
static int period_event = 0;                /* produce poll event after each period */
static int midi_note_a = 69;
static double note_a_freq = 440.;
static double octave = 12.;
static double max_phase = M_PI * 2.;
static double attack_time_secs = .2;  
static double decay_time_secs = .5;
static double release_time_secs = .5;
static double attack_step; 
static double decay_step;  
static double release_step;
static double sustain_gain = 0.7;
static double max_gain = 1.;
static double min_gain = 0.;
static snd_pcm_t *pcm_handle;
static snd_pcm_hw_params_t *hwparams;
static snd_pcm_sw_params_t *swparams;

static snd_pcm_sframes_t buffer_size;
static snd_pcm_sframes_t period_size;
static int bytes_per_sample;
static unsigned int max_sample_val; 
static snd_output_t *output = NULL;
static int set_hwparams(snd_pcm_t *handle, snd_pcm_hw_params_t *hwparams, snd_pcm_access_t access);
static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams);
static signed short *samples;
static snd_pcm_channel_area_t *areas;
// SPSC lock-free ring buffer
static spsc_ringbuf_t midi_evt_buf;
static void print_snd_pcm_hwparams(snd_pcm_hw_params_t *params);
static void print_snd_pcm_swparams(snd_pcm_sw_params_t *params); 
static double map_value(double in_start, double in_end, double out_start, double out_end, double value);
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

typedef enum {
    IDLE,
    ATTACK,
    DECAY,
    SUSTAIN,
    RELEASE,
} ADSR_T;

#define MAX_VOICES 128
typedef struct {
    uint8_t active;
    double freq;
    double phase;
    double phase_step;
    double amplitude;
    double gain;
    ADSR_T adsr_type;
} Voice;
static Voice voices[MAX_VOICES] = {0}; 

bool push_midi_evt(MidiEvent *me);
void *midi_evt_handler(void *arg);
#endif // SYNTH_H
