#include <alsa/asoundlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include "synth.h"
#define MAX_SEQ_EVTS 1024

static void receive_seq_events(snd_seq_t *seq);
int main(void)
{
    int rc, err, port, seq_client_id;
    const char *seq_name;
    printf("hello, world of synths\n");
    snd_seq_t *seq_handle;
    pthread_t synth_thread;
    const char *keyboard = "Keystation 61 MK3";

    rc = snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_INPUT, 0);
    snd_seq_set_client_name(seq_handle, "MySynth");
    port = snd_seq_create_simple_port(seq_handle, "MIDI Input", SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE, SND_SEQ_PORT_TYPE_APPLICATION);
    if (port < 0) {
        fprintf(stderr, "Failed to create sequencer port: %s\n", snd_strerror(port));
        return 1;
    } else {
        printf("My port = %d\n", port);
    }
    
    if (rc != 0)
    {
        perror(strerror(errno));
        return 1;
    } else {
        printf("snd_seq_open'd\n");
    }
    
    seq_name = snd_seq_name(seq_handle);
    seq_client_id = snd_seq_client_id(seq_handle);
    printf("snd_seq_id = %d\nsnd_seq_name = %s\n", seq_client_id, seq_name);
   
    err = snd_seq_connect_from(seq_handle, port, 20, 0);
    if (err < 0)
    {
        fprintf(stderr, "Cannot connect MIDI Input: %s\n", snd_strerror(err));
        return 1;
    } else {
        printf("sequencer connected to MIDI %s\n", seq_name);
    }
    
    if ((err = init_synth()) != 0) {
        fprintf(stderr, "init_synth failed %d\n", err);
        goto clean_up;
    }
    pthread_create(&synth_thread, NULL, midi_evt_handler, NULL);
    receive_seq_events(seq_handle);
    pthread_join(synth_thread, NULL);
clean_up:
    snd_seq_close(seq_handle);
    return 0;
}

static void receive_seq_events(snd_seq_t *seq)
{
    snd_seq_event_t *ev;
    int event_count = 0;

    while (snd_seq_event_input(seq, &ev) >= 0 && event_count < MAX_SEQ_EVTS)
    {
        char *onoff;
        MidiEvent me = {0};
        switch(ev->type)
        {
            case SND_SEQ_EVENT_NOTEON:
            case SND_SEQ_EVENT_NOTEOFF:
                onoff = ev->type == SND_SEQ_EVENT_NOTEON ? "ON" : "OFF";
                printf("NOTE %s: ch=%d note=%d velocity=%d\n", onoff, ev->data.note.channel, ev->data.note.note, ev->data.note.velocity, 0);
                me.type == SND_SEQ_EVENT_NOTEON ? 0 : 1; 
                me.channel = ev->data.note.channel; 
                me.note = ev->data.note.note; 
                me.velocity = ev->data.note.velocity, 
                me.value = 0;
                push_midi_evt(&me); 
                break;
            case SND_SEQ_EVENT_PITCHBEND:
                printf("PITCH BEND: ch=%d value=%d\n", ev->data.control.channel, ev->data.control.value);
                me.type = 2; 
                me.channel = ev->data.control.channel; 
                me.value = ev->data.control.value;
                push_midi_evt(&me);
                break;
            default:
                event_count = MAX_SEQ_EVTS;
                printf("UNKNOWN_CTRL: id=%d ch=%d value=%d\n", ev->type, ev->data.control.channel, ev->data.control.value);
                me.type = 3;
                push_midi_evt(&me);
                break;
        }
    }
}
