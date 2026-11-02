#include "./synth.h"
#include "./synth_lame.h"
#include "./synth_pcm.h"
#include "./spsc_ring_buffer.h"
#include "DList/dlist.h"

static double map_value(double in_start, double in_end, double out_start, double out_end, double value);
static double saw_tooth(double x); 
static double triangle_wave(double x);
// SPSC lock-free ring buffer
static spsc_ringbuf_t midi_evt_buf;
static DList *list;

static int synth_init() {
    int err;
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
    if (list == NULL) {
        return 1;
    }
    dlist_init(list, NULL);

    if (!record_samples) {
        return 0;
    } else if ((err = synth_lame_codec_configure(synth_pcm_period_size(), 1)) != 0) {
        record_samples = 0;
        return 0;
    }
    if ((out_file = fopen(out_filename, "wb")) == NULL) {
        record_samples = 0;
        return 0;
    }
    return 0;
}

bool push_midi_evt(MidiEvent *me) {
    return spsc_push(&midi_evt_buf, me, sizeof(MidiEvent));
}

void *midi_evt_handler(void *arg) {
    int rc;
    spsc_init(&midi_evt_buf);
    if ((rc = synth_pcm_init()) != 0) {
        return NULL;
    }
    synth_init();
    double sample = 0.0;
    struct timespec start, end;
    DListElmt *list_el; 
    Voice *cur_voice;
    short *samples = synth_pcm_samples();
    long period_size = synth_pcm_period_size();
    unsigned int max_sample_val = synth_pcm_max_sample();
    size_t pcm_byte_count;

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
                    sample += sin(cur_voice->phase) * cur_voice->amplitude * cur_voice->gain;
                    // sample += saw_tooth(cur_voice->phase) * cur_voice->amplitude * cur_voice->gain;
                    // sample += triangle_wave(cur_voice->phase) * cur_voice->amplitude * cur_voice->gain;
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
            synth_pcm_sample(i, (signed short)(tanh(sample) * max_sample_val));
        }

        rc = synth_pcm_write();
        if (rc == -EAGAIN) {
            printf("continue\n");
            continue;
        } 
        if (rc < 0) {
            if (synth_pcm_xrun_recovery(rc) < 0) {
                printf("Write error: %s\n", snd_strerror(rc));
                break;
            }
            continue;  /* skip one period */
        }
        if (record_samples) {
            if ((rc = synth_lame_encode_buffer(synth_pcm_samples())) < 0) {
                fprintf(stderr, "ERROR: lame_encode_buffer %d\n", rc);
                continue;
            } 
            if ((rc = fwrite((void *)synth_lame_mp3_buffer(), sizeof(unsigned char), rc, out_file)) < 0) {
                fprintf(stderr, "ERROR: fwrite lame buffer %d\n", rc);
            }
        }

    }
    printf("freq_handler freed resources\n");
    dlist_destroy(list);
    if (record_samples) {
        if ((rc = synth_lame_recording_flush()) > 0) {
            fwrite((void *)synth_lame_mp3_buffer(), sizeof(unsigned char), rc, out_file);
        }
        synth_lame_close();
        fclose(out_file);
    }
    return NULL;
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


