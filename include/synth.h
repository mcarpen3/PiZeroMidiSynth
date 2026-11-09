#ifndef SYNTH_H
#define SYNTH_H

#include <stdbool.h>
#include <inttypes.h>
#include <math.h>
#include "stdio.h"

static int verbose = 1;
static int midi_note_a = 69;
static double note_a_freq = 440.;
static double octave = 12.;
static double max_phase = M_PI * 2.;
static double attack_time_secs = .2;  
static double decay_time_secs = .3;
static double release_time_secs = .4;
static double tempo_bpm = 60.;        // .4 * 60 = 24bpm
static double attack_step; 
static double decay_step;  
static double release_step;
static double tempo_step;
static double sustain_gain = 0.7;
static double max_gain = 1.;
static double min_gain = 0.;
static int record_samples = 1;
static FILE *out_file;
static const char *out_filename = "./recording.mp3";

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

#define MAX_NOTES 128
#define MAX_VOICES 12
typedef struct {
    uint8_t active;
    uint8_t note;
    double freq;
    double phase;
    double phase_step;
    double amplitude;
    double gain;
    ADSR_T adsr_type;
} Voice;
static Voice voices[MAX_NOTES] = {0};

typedef struct {
    double phase;
    double gain;
    ADSR_T adsr_type;
} Tempo;
static Tempo tempo = {0};
bool push_midi_evt(MidiEvent *me);
void *midi_evt_handler(void *arg);

#endif // SYNTH_H
