#include "./synth.h"
static int xrun_recovery(snd_pcm_t *handle, int err);
static void print_snd_pcm_hwparams(snd_pcm_hw_params_t *params);
static void print_snd_pcm_swparams(snd_pcm_sw_params_t *params);
static void print_snd_pcm(snd_pcm_t *handle);
static int set_hwparams(snd_pcm_t *handle, snd_pcm_hw_params_t *params, snd_pcm_access_t access);
static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *params);


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
    attack_step  = 1 / attack_time_secs / rate;
    decay_step   = 1 / decay_time_secs / rate;
    release_step = 1 / release_time_secs / rate;
    tempo_step   = 1 / (tempo_bpm / 120.) / rate;
    printf("[SYNTH] attack_step: %f, decay_step: %f, release_step: %f\n", attack_step, decay_step, release_step);
    printf("[SYNTH] Initializing %d voices...\n", MAX_NOTES);
    for (uint8_t i = 0; i < MAX_NOTES; i++) {
        int relative_note = (int)i - midi_note_a;
        voices[i].freq = note_a_freq * pow(2.0, relative_note / octave);
        voices[i].phase_step = max_phase * voices[i].freq / rate;
        voices[i].adsr_type = IDLE;
        voices[i].gain = min_gain;
    }
    tempo.adsr_type = IDLE;
    tempo.gain      = 0.;
    tempo.phase     = 0.;

    printf("[SYNTH] %d NOTES initialized.\n", MAX_NOTES);
    printf("[SYNTH] intializing active voice LIST\n");
    list = malloc(sizeof(DList));
    dlist_init(list, NULL);
    if (verbose) {
        print_snd_pcm(pcm_handle);
        print_snd_pcm_hwparams(hwparams);
        print_snd_pcm_swparams(swparams);
        printf("MAX_SAMPLE_VAL: %d\n", max_sample_val);
        printf("SAMPLE FMT UNSIGNED: %s\n", snd_pcm_format_unsigned(format) ? "TRUE" : "FALSE");
        printf("SAMPLE FMT BIGENDIAN: %s\n", snd_pcm_format_big_endian(format) ? "TRUE" : "FALSE");
        printf("SAMPLE FMT FLOAT: %s\n", snd_pcm_format_big_endian(format) ? "TRUE" : "FALSE");
    }

    return 0;
}

bool push_midi_evt(MidiEvent *me) {
    return spsc_push(&midi_evt_buf, me, sizeof(MidiEvent));
}

static double map_value(double in_start, double in_end, double out_start, double out_end, double value) {
    return ((value - in_start)/(in_end - in_start) * (out_end - out_start) + out_start);
}

static double saw_tooth(double x) {
    double a = abs(x);
    int k = floor(x * M_PI / 2);
    double v = x - k * M_PI * 0.5;
    double y;
    int quadrant = k % 4;
    switch(quadrant) {
        case 0:
        case 2:
            y = map_value(v, 0., M_PI / 2, 1., 0.);
            break;
        case 1:
        case 3:
            y = map_value(v, 0., M_PI / 2, 0, -1.);
            break;
    }
    return x < 0 ? -y : y;
}

static double triangle_wave(double x) {
    double a = abs(x);
    int k = floor(x * M_PI / 2);
    double v = x - k * M_PI * 0.5;
    double y;
    int quadrant = k % 4;
    switch(quadrant) {
        case 0:
            y = map_value(v, 0., M_PI / 2, 0., -1.);
            break;
        case 1:
            y = map_value(v, 0., M_PI / 2, -1., 0.);
            break;
        case 2:
            y = map_value(v, 0., M_PI / 2, 0., 1.);
            break;
        case 3:
            y = map_value(v, 0., M_PI / 2, 1., 0.);
            break;
    }
    return x < 0 ? -y : y;
}

void *midi_evt_handler(void *arg) {
    double sample = 0.0;
    struct timespec start, end;
    DListElmt *list_el; 
    Voice *cur_voice;

    while(1) {
        MidiEvent me;
        if (spsc_pop(&midi_evt_buf, &me, sizeof(me))) {
            if (me.type == 3) {
                break;
            }
            switch(me.type) {
                case NOTE_ON:
                    if (dlist_size(list) == MAX_VOICES) {
                        break;
                    } else if (voices[me.note].adsr_type == IDLE) {
                        dlist_ins_next(list, dlist_tail(list), &voices[me.note]);
                    }
                    voices[me.note].adsr_type = ATTACK;
                    voices[me.note].gain = min_gain;
                    voices[me.note].amplitude = me.velocity / 127.0;
                    break;
                case NOTE_OFF:
                    if (voices[me.note].adsr_type != IDLE) {
                        voices[me.note].adsr_type = RELEASE;
                        voices[me.note].gain = sustain_gain;
                    }
                    break;
                default:
                    break;
            }
            if (verbose) 
                printf("[SYNTH] NOTE#=%d VOICE_COUNT=%u VOICE_ADSR=%d\n", me.note, dlist_size(list), voices[me.note].adsr_type);
         }
         for (int i = 0; i < period_size; i++) {
            sample = 0.0;
            // sample += fmin(1, fmax(-1., tan(tempo.phase)));
            tempo.phase += tempo_step;
            if (tempo.phase >= 1) tempo.phase = 0;
            list_el = dlist_head(list);
            while (list_el != NULL) {
                cur_voice = (Voice *)dlist_data(list_el);
                if (cur_voice->adsr_type != IDLE) {
                    // sample += sin(cur_voice->phase) * cur_voice->amplitude * cur_voice->gain;
                    // sample += saw_tooth(cur_voice->phase) * cur_voice->amplitude * cur_voice->gain;
                    sample += triangle_wave(cur_voice->phase) * cur_voice->amplitude * cur_voice->gain;
                    cur_voice->phase += cur_voice->phase_step;
                    switch(cur_voice->adsr_type) {
                        case ATTACK:
                            cur_voice->gain += attack_step;
                            if (cur_voice->gain >= max_gain) {
                                cur_voice->adsr_type = DECAY;
                            }
                            break;
                        case DECAY:
                            cur_voice->gain -= decay_step;
                            if (cur_voice->gain <= sustain_gain) {
                                cur_voice->adsr_type = SUSTAIN;
                            }
                            break;
                        case RELEASE:
                            cur_voice->gain -= release_step;
                            if (cur_voice->gain <= min_gain) {
                                cur_voice->adsr_type = IDLE;
                                Voice *rem_voice;
                                dlist_remove(list, list_el, (void **)&rem_voice);
                            } 
                            break;
                        default:
                            break;
                    }
                }

                if (cur_voice->phase >= max_phase) {
                    cur_voice->phase -= max_phase;
                }
                list_el = dlist_next(list_el);
            }
            samples[i] = (signed short)(tanh(sample) * max_sample_val);
        }

        int err = snd_pcm_writei(pcm_handle, (void *)samples, period_size);
        if (err == -EAGAIN) {
            printf("continue\n");
            continue;
        } 
        if (err < 0) {
            if (xrun_recovery(pcm_handle, err) < 0) {
                printf("Write error: %s\n", snd_strerror(err));
                break;
            }
            continue;  /* skip one period */
        }
    }
    free(areas);
    free(samples);
    snd_pcm_close(pcm_handle);
    printf("freq_handler freed resources\n");
    dlist_destroy(list);
    return NULL;
}
/*
 *   Underrun and suspend recovery
 */

static int xrun_recovery(snd_pcm_t *handle, int err)
{
    if (verbose)
        printf("stream recovery\n");
    if (err == -EPIPE) {    /* under-run */
        err = snd_pcm_prepare(handle);
        if (err < 0)
            printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
        return 0;
    } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(handle)) == -EAGAIN)
            sleep(1);   /* wait until the suspend flag is released */
        if (err < 0) {
            err = snd_pcm_prepare(handle);
            if (err < 0)
                printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
        }
        return 0;
    }
    return err;
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

static void print_snd_pcm(snd_pcm_t *handle) {
    printf("--- snd_pcm_dump ---\n");
    snd_output_t *output;
    snd_output_stdio_attach(&output, stdout, 0);
    snd_pcm_dump(handle, output);
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

