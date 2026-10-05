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
    
    printf("[SYNTH] Initializing %d voices...\n", MAX_VOICES);
    for (int i = 0; i < MAX_VOICES; i++) {
        int relative_note = i - midi_note_a;
        voices[i].freq = note_a_freq * pow(2.0, relative_note / octave);
        voices[i].phase_step = max_phase * voices[i].freq / rate;
        voices[i].attack_phase = 0;
        voices[i].attack_phase_step = max_attack_phase * attack_freq / rate;
        voices[i].decay_phase = M_PI * 0.5;
        voices[i].decay_phase_step = max_attack_phase * decay_freq / rate; 
    }
    printf("[SYNTH] %d voices initialized.\n", MAX_VOICES);
    
    print_snd_pcm_hwparams(hwparams);
    print_snd_pcm_swparams(swparams);
    printf("MAX_SAMPLE_VAL: %d\n", max_sample_val);
    printf("SAMPLE FMT UNSIGNED: %s\n", snd_pcm_format_unsigned(format) ? "TRUE" : "FALSE");
    printf("SAMPLE FMT BIGENDIAN: %s\n", snd_pcm_format_big_endian(format) ? "TRUE" : "FALSE");
    printf("SAMPLE FMT FLOAT: %s\n", snd_pcm_format_big_endian(format) ? "TRUE" : "FALSE");

    return 0;
}

bool push_midi_evt(MidiEvent *me) {
    return spsc_push(&midi_evt_buf, me, sizeof(MidiEvent));
}

static double map_value(double in_start, double in_end, double out_start, double out_end, double value) {
    return ((value - in_start)/(in_end - in_start) * (out_end - out_start) + out_start);
}

static double normal_phase(double phase) {
    return map_value(-1., 1., 0., 1., phase);
}
            
void *midi_evt_handler(void *arg) {
    double sample = 0.0;
    unsigned short voice_count = 0;
    double norm_attack_phase = 0.;
    double norm_decay_phase = 0.;
    while(1) {
        MidiEvent me;
        if (spsc_pop(&midi_evt_buf, &me, sizeof(me))) {
            if (me.type == 3) {
                break;
            }
            voices[me.note].active = me.type == NOTE_ON ? 1 : 0;
            voices[me.note].amplitude = me.velocity / 127.0;
            voices[me.note].attack_phase = -1.;
            voices[me.note].decay_phase = M_PI * .5;
            printf("[SYNTH] freq: %f, phase_step: %f, phase: %f, decay_phase: %f, decay_phase_step: %f\n", 
                    voices[me.note].freq, voices[me.note].phase_step, voices[me.note].phase,
                    voices[me.note].decay_phase, voices[me.note].decay_phase_step);
         }
         for (int i = 0; i < period_size; i++) {
            sample = 0.0;
            voice_count = 0;
            for (int j = 0; j < MAX_VOICES; j++) {
                if (voices[j].active) {
                    voice_count++;
                    norm_attack_phase = normal_phase(sin(voices[j].attack_phase)); 
                    sample += sin(voices[j].phase) * voices[j].amplitude * norm_attack_phase;
                    voices[j].phase += voices[j].phase_step;
                    voices[j].attack_phase += voices[j].attack_phase_step;
                    if (voices[j].phase >= max_phase) {
                        voices[j].phase -= max_phase;
                    }
                    if (voices[j].attack_phase >= max_attack_phase) {
                         voices[j].attack_phase = max_attack_phase;
                    }

                } else if (voices[j].decay_phase > 0.) {
                    // printf("voice[%d].decay_phase = %f\n", j, voices[j].decay_phase);
                    voice_count++;
                    norm_decay_phase = normal_phase(sin(voices[j].decay_phase));
                    sample += sin(voices[j].phase) * voices[j].amplitude * norm_decay_phase;
                    voices[j].phase += voices[j].phase_step;
                    voices[j].decay_phase -= voices[j].decay_phase_step;
                    if (voices[j].phase >= max_phase) {
                        voices[j].phase -= max_phase;
                    }
                }
            }
            samples[i] = (signed short)(tanh(sample) * max_sample_val);
            // samples[i] = (signed short)(sample * max_sample_val / voice_count);
        }
        int err = snd_pcm_writei(pcm_handle, (void *)samples, period_size);
        if (err == -EAGAIN) {
            continue;
        } else if (err < 0) {
            fprintf(stderr, "%s\n", snd_strerror(err));
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

