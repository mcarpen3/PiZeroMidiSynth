#include "./synth_pcm.h"

int synth_pcm_init(void) {
    int err;
    snd_pcm_hw_params_alloca(&hwparams);
    snd_pcm_sw_params_alloca(&swparams);
    if ((err = snd_pcm_open(&pcm_handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        return 1;
    }

    if ((err = synth_pcm_set_hwparams(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        printf("Setting of hwparams failed: %s\n", snd_strerror(err));
        return 1;
    }
    if ((err = synth_pcm_set_swparams(pcm_handle, swparams)) < 0) {
        printf("Setting of swparams failed: %s\n", snd_strerror(err));
        return 1;
    }

    int sample_bytes = (period_size * channels * snd_pcm_format_physical_width(format)) / 8;
    samples = malloc(sample_bytes);
    if (samples == NULL) {
        printf("No enough memory\n");
        return 1;
    }
    memset(samples, 0, sample_bytes);

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
    
    bytes_per_sample = snd_pcm_format_width(format) / 8;
    max_sample_val = (1 << snd_pcm_format_width(format) - 1) - 1;
        
    if (synth_pcm_verbose) {
        synth_pcm_print_snd_pcm(pcm_handle);
        synth_pcm_print_hwparams(hwparams);
        synth_pcm_print_swparams(swparams);
        printf("MAX_SAMPLE_VAL: %d\n", max_sample_val);
        printf("SAMPLE FMT UNSIGNED: %s\n", snd_pcm_format_unsigned(format) ? "TRUE" : "FALSE");
        printf("SAMPLE FMT BIGENDIAN: %s\n", snd_pcm_format_big_endian(format) ? "TRUE" : "FALSE");
        printf("SAMPLE FMT FLOAT: %s\n", snd_pcm_format_big_endian(format) ? "TRUE" : "FALSE");
    }
    return 0;
}

int synth_pcm_write() {
    return snd_pcm_writei(pcm_handle, samples, period_size);
}

void synth_pcm_sample(int idx, signed short sample) {
    samples[idx] = sample;
}

int synth_pcm_set_hwparams(snd_pcm_t *handle, snd_pcm_hw_params_t *params, snd_pcm_access_t access) {
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

int synth_pcm_set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams)
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

void synth_pcm_print_hwparams(snd_pcm_hw_params_t *params)
{
    printf("--- snd_pcm_hw_params ---\n");
    snd_output_t *output;
    snd_output_stdio_attach(&output, stdout, 0);
    snd_pcm_hw_params_dump(params, output);
    snd_output_close(output);
    printf("-------------------------\n");
}

void synth_pcm_print_swparams(snd_pcm_sw_params_t *params) {
    printf("--- snd_pcm_sw_params ---\n");
    snd_output_t *output;
    snd_output_stdio_attach(&output, stdout, 0);
    snd_pcm_sw_params_dump(params, output);
    snd_output_close(output);
    printf("-------------------------\n");
}

void synth_pcm_print_snd_pcm(snd_pcm_t *handle) {
    printf("--- snd_pcm_dump ---\n");
    snd_output_t *output;
    snd_output_stdio_attach(&output, stdout, 0);
    snd_pcm_dump(handle, output);
    snd_output_close(output);
    printf("-------------------------\n");
}

/*
 *   Underrun and suspend recovery
 */

int synth_pcm_xrun_recovery(int err)
{
    if (synth_pcm_verbose)
        printf("stream recovery\n");
    if (err == -EPIPE) {    /* under-run */
        err = snd_pcm_prepare(pcm_handle);
        if (err < 0)
            printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
        return 0;
    } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(pcm_handle)) == -EAGAIN)
            sleep(1);   /* wait until the suspend flag is released */
        if (err < 0) {
            err = snd_pcm_prepare(pcm_handle);
            if (err < 0)
                printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
        }
        return 0;
    }
    return err;
}


short *synth_pcm_samples() {
    return samples;
}

long synth_pcm_period_size() {
    return period_size;
}

void synth_pcm_shutdown() {
    free(areas);
    free(samples);
    snd_pcm_close(pcm_handle);
}

unsigned int synth_pcm_max_sample() {
    return max_sample_val;
}

unsigned int synth_pcm_channels() {
    return channels;
}
