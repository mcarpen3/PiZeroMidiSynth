#ifndef SYNTH_PCM_H
#define SYNTH_PCM_H
#include <alsa/asoundlib.h>

static char *device = "plughw:CARD=IQaudIOCODEC,DEV=0";         /* playback device */
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
static unsigned int period_time = 10000;    // microseconds
static double freq = 440;                   /* sinusoidal wave frequency in Hz */
static int synth_pcm_verbose = 1;                     /* verbose flag */
static int resample = 1;                    /* enable alsa-lib resampling */
static int period_event = 0;                /* produce poll event after each period */

static snd_pcm_t *pcm_handle;
static snd_pcm_hw_params_t *hwparams;
static snd_pcm_sw_params_t *swparams;

static snd_pcm_sframes_t buffer_size;
static snd_pcm_sframes_t period_size;
static int bytes_per_sample;
static unsigned int max_sample_val; 

int synth_pcm_init(void);
void synth_pcm_print_hwparams(snd_pcm_hw_params_t *params);
void synth_pcm_print_swparams(snd_pcm_sw_params_t *params);
void synth_pcm_print_snd_pcm(snd_pcm_t *handle);
int synth_pcm_set_hwparams(snd_pcm_t *handle, snd_pcm_hw_params_t *params, snd_pcm_access_t access);
int synth_pcm_set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *params);
int synth_pcm_xrun_recovery(int err);
int synth_pcm_write();
void synth_pcm_sample(int idx, signed short sample);

short *synth_pcm_samples();
snd_pcm_sframes_t synth_pcm_period_size();

static signed short *samples;
static snd_output_t *output = NULL;
static snd_pcm_channel_area_t *areas;
void synth_pcm_shutdown();

#endif // SYNTH_PCM_H

