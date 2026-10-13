#include <alsa/asoundlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include "synth.h"
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>

#define MAX_SEQ_EVTS 1024

static void receive_seq_events(snd_seq_t *seq);
static void std_keyboard_event_handler(); // fallback to 
static int sequencer_client_setup(snd_seq_t **seq);
static void drain_seq_fifo_and_buffer(snd_seq_t *seq);

int main(int argc, char *argv[])
{
    int rc, port, use_midi=1;
    const char *seq_name;
    printf("Hello, world of Midi and Synths!!!\n");
    snd_seq_t *seq_handle = NULL;
    pthread_t synth_thread;

    if (argc == 2) {
        use_midi = 0;
        printf("Using standard computer keyboard\n");
    } else if ((rc = sequencer_client_setup(&seq_handle)) != 0) {
        fprintf(stderr, "Failed to create sequencer client: %s\n", snd_strerror(rc));
        seq_handle = NULL;
        goto clean_up;
    } else {
        printf("Using Keystation 61 MK3\n");
    }

    if ((rc = init_synth()) != 0) {
        fprintf(stderr, "init_synth failed %s\n", snd_strerror(rc));
        goto clean_up;
    }
    
    if ((rc = pthread_create(&synth_thread, NULL, midi_evt_handler, NULL) != 0)) {
        goto clean_up;
    }

        
    if (use_midi) {
        receive_seq_events(seq_handle);
    } else {
        std_keyboard_event_handler();
    }

    pthread_join(synth_thread, NULL);
clean_up:
    if (seq_handle) {
        snd_seq_close(seq_handle);
        printf("Midi sequencer handle closed\n");
    }
    return 0;
}
static void drain_seq_fifo_and_buffer(snd_seq_t *seq) {
    int event_count;
    snd_seq_event_t *ev;
    // Drain the sequencer FIFO
    event_count = snd_seq_event_input_pending(seq, 1);
    printf("[SEQ] Draining %d events in seq FIFO\n", event_count);
    do {
        event_count = snd_seq_event_input_pending(seq, 0);
        printf("[SEQ] Draining %d bytes in seq buffer\n", event_count);
        snd_seq_event_input(seq, &ev);
    } while (event_count > 0);
}

static void receive_seq_events(snd_seq_t *seq)
{
    snd_seq_event_t *ev;
    int exit = 0;
    MidiEvent me = {0};
    drain_seq_fifo_and_buffer(seq);    
    while (snd_seq_event_input(seq, &ev) >= 0 && !exit)
    {
        char *onoff;
        switch(ev->type)
        {
            case SND_SEQ_EVENT_NOTEON:
            case SND_SEQ_EVENT_NOTEOFF:
                onoff = ev->type == SND_SEQ_EVENT_NOTEON ? "ON" : "OFF";
                printf("[SEQ] NOTE %s: ch=%d note=%d velocity=%d\n", onoff, ev->data.note.channel, ev->data.note.note, ev->data.note.velocity);
                me.type = ev->type == SND_SEQ_EVENT_NOTEON ? 0 : 1; 
                me.channel = ev->data.note.channel; 
                me.note = ev->data.note.note; 
                me.velocity = ev->data.note.velocity, 
                me.value = 0;
                break;
            case SND_SEQ_EVENT_PITCHBEND:
                printf("[SEQ] PITCH BEND: ch=%d value=%d\n", ev->data.control.channel, ev->data.control.value);
                me.type = 2; 
                me.channel = ev->data.control.channel; 
                me.value = ev->data.control.value;
                break;
            default:
                printf("[SEQ] UNKNOWN_CTRL: id=%d ch=%d value=%d\n", ev->type, ev->data.control.channel, ev->data.control.value);
                me.type = 3;
                exit = 1;
                break;
        }
        push_midi_evt(&me); 
    }
    drain_seq_fifo_and_buffer(seq);
}

static void std_keyboard_event_handler() {
    const char *input_device = "/dev/input/event2";
    int fd = open(input_device, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open input device ");
        return;
    }
    struct input_event ev;
    MidiEvent me = {0};
    while(1) {
        read(fd, &ev, sizeof(struct input_event));
        
        if (ev.type == EV_KEY && ev.value <= 1) {
            me.type = ev.code == 1 
                ? 3 
                :     ev.value == 1 ? 0 : 1;
            me.channel = 0;
            me.note = ev.code + 32;
            me.velocity = 80; 
            me.value = 0;
            push_midi_evt(&me);
            if (ev.code == 1) {
                break;
            }
        }
    }
    close(fd);
    return;
}

static int sequencer_client_setup(snd_seq_t **handle) {
    int rc, port, seq_client_id;
    const char *seq_name;
    // Try and get a handle on the ALSA system's default MIDI sequencer handle
    // When my M-AUDIO Keystation 61 is connected, an alsa sequencer driver is attached TODO learn more about snd_usb_audio driver   
    // command-lines like "aseqdump -l" and "aseqdump -p <portnum of midi device>" are useful tools
    rc = snd_seq_open(handle, "default", SND_SEQ_OPEN_INPUT, 0);
    if (rc != 0) {
        return rc;
    } else {
        printf("snd_seq_open'd\n");
    }

    // Sequencer client setup
    // The bitwise flags specify what other sequencers have the capability to do
    // Assuming these client setup routines succeed, then running "aconnect -l" in another terminal will show the client registered on the ALSA sequencer
    snd_seq_set_client_name(*handle, "MySynth");
    port = snd_seq_create_simple_port(*handle, "MIDI Input", SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE, SND_SEQ_PORT_TYPE_APPLICATION);
    if (port < 0) {
        return port;
    } else {
        printf("Midi sequencer port = %d\n", port);
    }
    
    seq_name = snd_seq_name(*handle);
    seq_client_id = snd_seq_client_id(*handle);
    printf("snd_seq_id = %d\nsnd_seq_name = %s\n", seq_client_id, seq_name);
   
    rc = snd_seq_connect_from(*handle, port, 20, 0);
    if (rc < 0)
    {
        return rc;
    } else {
        printf("Midi sequencer connected to %s\n", seq_name);
    }
    return 0;
}

