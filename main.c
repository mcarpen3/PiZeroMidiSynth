#include <alsa/asoundlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#define MAX_SEQ_EVTS 1024

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

static snd_pcm_sframes_t buffer_size;
static snd_pcm_sframes_t period_size;
static snd_output_t *output = NULL;

static void receive_seq_events(snd_seq_t *seq);
static int set_hwparams(snd_pcm_t *handle, snd_pcm_hw_params_t *hwparams, snd_pcm_access_t access);
static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams);
static void *play_note(void *args);

int main(void)
{
    int rc, err, port, seq_client_id;
    const char *seq_name;
    printf("hello, world of synths\n");
    snd_seq_t *seq_handle;
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *hwparams;
    snd_pcm_sw_params_t *swparams;
    signed short *samples;
    snd_pcm_channel_area_t *areas;
    pthread_t play_note_thread;

    snd_pcm_hw_params_alloca(&hwparams);
    snd_pcm_sw_params_alloca(&swparams);
    if ((err = snd_pcm_open(&pcm_handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        return 0;
    }

    if ((err = set_hwparams(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        printf("Setting of hwparams failed: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }
    if ((err = set_swparams(pcm_handle, swparams)) < 0) {
        printf("Setting of swparams failed: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }


    const char *keyboard = "Keystation 61 MK3";

    rc = snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_INPUT, 0);
    snd_seq_set_client_name(seq_handle, "MySynth");
    port = snd_seq_create_simple_port(seq_handle, "MIDI Input", SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE, SND_SEQ_PORT_TYPE_APPLICATION);
    if (port < 0) {
        fprintf(stderr, "Failed to create sequencer port: %s\n", snd_strerror(port));
        return 1;
    }
    //snd_seq_system_info_t *seq_sys_info;
    //snd_seq_system_info_malloc(&seq_sys_info);
    //snd_seq_system_info_free(seq_sys_info);
    //snd_seq_system_info_get_clients(seq_sys_info);
    if (rc != 0)
    {
        perror(strerror(errno));
        return 1;
    }
    
    seq_name = snd_seq_name(seq_handle);
    seq_client_id = snd_seq_client_id(seq_handle);
    printf("snd_seq_id = %d\nsnd_seq_name = %s\n", seq_client_id, seq_name);
   
    err = snd_seq_connect_from(seq_handle, port, 20, 0);
    if (err < 0)
    {
        fprintf(stderr, "Cannot connect MIDI Input: %s\n", snd_strerror(err));
        return 1;
    }
    
    samples = malloc((period_size * channels * snd_pcm_format_physical_width(format)) / 8);
    if (samples == NULL) {
        printf("No enough memory\n");
        exit(EXIT_FAILURE);
    }

    areas = calloc(channels, sizeof(snd_pcm_channel_area_t));
    if (areas == NULL) {
        printf("No enough memory\n");
        exit(EXIT_FAILURE);
    }
    for (int chn = 0; chn < channels; chn++) {
        areas[chn].addr = samples;
        areas[chn].first = chn * snd_pcm_format_physical_width(format);
        areas[chn].step = channels * snd_pcm_format_physical_width(format);
    }

    pthread_create(&play_note_thread, NULL, play_note, NULL);
    receive_seq_events(seq_handle);
    snd_seq_close(seq_handle);
    snd_pcm_close(pcm_handle);
    return 0;
}

static void receive_seq_events(snd_seq_t *seq)
{
    snd_seq_event_t *ev;
    int event_count = 0;

    while (snd_seq_event_input(seq, &ev) >= 0 && event_count < MAX_SEQ_EVTS)
    {
        switch(ev->type)
        {
            case SND_SEQ_EVENT_NOTEON:
                printf("NOTE ON: ch=%d note=%d velocity=%d\n", ev->data.note.channel, ev->data.note.note, ev->data.note.velocity);
                freq = 440.0 * pow(2.0, (ev->data.note.note - 69) / 12.0);
                break;
            case SND_SEQ_EVENT_NOTEOFF:
                printf("NOTE OFF: ch=%d note=%d velocity=%d\n", ev->data.note.channel, ev->data.note.note, ev->data.note.velocity);
                freq = 0;
                break;
            case SND_SEQ_EVENT_PITCHBEND:
                printf("PITCH BEND: ch=%d value=%d\n", ev->data.control.channel, ev->data.control.value);
                break;
            default:
                event_count = MAX_SEQ_EVTS;
                printf("UNKNOWN_CTRL: id=%d ch=%d value=%d\n", ev->type, ev->data.control.channel, ev->data.control.value);
                break;
        }
    }
}

static void *play_note(void *args) {
    sleep(2);
    printf("play_note thread\n");
    return NULL;
}

static int set_hwparams(snd_pcm_t *handle,
            snd_pcm_hw_params_t *params,
            snd_pcm_access_t access)
{
    unsigned int rrate;
    snd_pcm_uframes_t size;
    int err, dir;

    /* choose all parameters */
    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0) {
        printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
        return err;
    }
    /* set hardware resampling */
    err = snd_pcm_hw_params_set_rate_resample(handle, params, resample);
    if (err < 0) {
        printf("Resampling setup failed for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the interleaved read/write format */
    err = snd_pcm_hw_params_set_access(handle, params, access);
    if (err < 0) {
        printf("Access type not available for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the sample format */
    err = snd_pcm_hw_params_set_format(handle, params, format);
    if (err < 0) {
        printf("Sample format not available for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the count of channels */
    err = snd_pcm_hw_params_set_channels(handle, params, channels);
    if (err < 0) {
        printf("Channels count (%u) not available for playbacks: %s\n", channels, snd_strerror(err));
        return err;
    }
    /* set the stream rate */
    rrate = rate;
    err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
    if (err < 0) {
        printf("Rate %uHz not available for playback: %s\n", rate, snd_strerror(err));
        return err;
    }
    if (rrate != rate) {
        printf("Rate doesn't match (requested %uHz, get %iHz)\n", rate, err);
        return -EINVAL;
    }
    /* set the buffer time */
    err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, &dir);
    if (err < 0) {
        printf("Unable to set buffer time %u for playback: %s\n", buffer_time, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_buffer_size(params, &size);
    if (err < 0) {
        printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
        return err;
    }
    buffer_size = size;
    /* set the period time */
    err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, &dir);
    if (err < 0) {
        printf("Unable to set period time %u for playback: %s\n", period_time, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
    if (err < 0) {
        printf("Unable to get period size for playback: %s\n", snd_strerror(err));
        return err;
    }
    period_size = size;
    /* write the parameters to device */
    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
        printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
        return err;
    }
    return 0;
}

static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams)
{
    int err;

    /* get the current swparams */
    err = snd_pcm_sw_params_current(handle, swparams);
    if (err < 0) {
        printf("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* start the transfer when the buffer is almost full: */
    /* (buffer_size / avail_min) * avail_min */
    err = snd_pcm_sw_params_set_start_threshold(handle, swparams, (buffer_size / period_size) * period_size);
    if (err < 0) {
        printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* allow the transfer when at least period_size samples can be processed */
    /* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
    err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_event ? buffer_size : period_size);
    if (err < 0) {
        printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* enable period events when requested */
    if (period_event) {
        err = snd_pcm_sw_params_set_period_event(handle, swparams, 1);
        if (err < 0) {
            printf("Unable to set period event: %s\n", snd_strerror(err));
            return err;
        }
    }
    /* write the parameters to the playback device */
    err = snd_pcm_sw_params(handle, swparams);
    if (err < 0) {
        printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
        return err;
    }
    return 0;
}

