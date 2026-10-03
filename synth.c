#include "./synth.h"

int init_synth(void) {
    int err;
    spsc_init(&midi_evt_buf);
    snd_pcm_hw_params_alloca(&hwparams);
    snd_pcm_sw_params_alloca(&swparams);
    if ((err = snd_pcm_open(&pcm_handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        return 1;
    }

    if ((err = set_hwparams(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        printf("Setting of hwparams failed: %s\n", snd_strerror(err));
        return 1;
    }
    if ((err = set_swparams(pcm_handle, swparams)) < 0) {
        printf("Setting of swparams failed: %s\n", snd_strerror(err));
        return 1;
    }

    samples = malloc((period_size * channels * snd_pcm_format_physical_width(format)) / 8);
    if (samples == NULL) {
        printf("No enough memory\n");
        return 1;
    }

    areas = calloc(channels, sizeof(snd_pcm_channel_area_t));
    if (areas == NULL) {
        printf("No enough memory\n");
        return 1;
    }
    for (int chn = 0; chn < channels; chn++) {
        areas[chn].addr = samples;
        areas[chn].first = chn * snd_pcm_format_physical_width(format);
        areas[chn].step = channels * snd_pcm_format_physical_width(format);
    }
    print_snd_pcm_hwparams(hwparams);
    print_snd_pcm_swparams(swparams);
    return 0;
}

bool push_midi_evt(MidiEvent *me) {
    return spsc_push(&midi_evt_buf, me, sizeof(MidiEvent));
}

void *midi_evt_handler(void *arg) {
    // read from spsc ring buffer
    //
    // freq = 440.0 * pow(2.0, (ev->data.note.note - 69) / 12.0);
    //
    static double max_phase = 2. * M_PI;
    while(1) {
        MidiEvent me;
        if (spsc_pop(&midi_evt_buf, &me, sizeof(me))) {
            double freq = 440.0 * pow(2.0, (me.note - 69) / 12.0);
            double step = max_phase * freq / rate;
            printf("note: %d, velocity: %d, freq: %f, step: %f\n", me.note, me.velocity, freq, step);
//            for (int i = 0; i < period_size; i++) {
//                if (me.
            if (me.type == 3) {
                break;
            }
        }
    }
    // snd_pcm_hw_params_free(hwparams); only necessary of params were malloc'ed note alloca'ed
    // snd_pcm_sw_params_free(swparams);
    free(areas);
    free(samples);
    snd_pcm_close(pcm_handle);
    printf("freq_handler freed resources\n");
    return NULL;
}

static void print_snd_pcm_hwparams(snd_pcm_hw_params_t *params)
{
    printf("--- snd_pcm_hw_params ---\n");
    snd_output_t *output;
    snd_output_stdio_attach(&output, stdout, 0);
    snd_pcm_hw_params_dump(params, output);
    snd_output_close(output);
    printf("-------------------------\n");
}

static void print_snd_pcm_swparams(snd_pcm_sw_params_t *params) {
    printf("--- snd_pcm_sw_params ---\n");
    snd_output_t *output;
    snd_output_stdio_attach(&output, stdout, 0);
    snd_pcm_sw_params_dump(params, output);
    snd_output_close(output);
    printf("-------------------------\n");
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

