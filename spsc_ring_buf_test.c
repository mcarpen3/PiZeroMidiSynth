#include "spsc_ring_buffer.h"
#include <stdio.h>
#include <pthread.h>
#include <limits.h>
#include "synth.h"

spsc_ringbuf_t rb = {0};
int a=0,b=1,c=2,d=3,e=4;

void *prod(void *arg) {
    for (int i = 0; i < 48; ) {
        MidiEvent me = {a,b,c,d,e};
        if (spsc_push(&rb, &me, sizeof(me))) {
            i++;
            a++; b++; c++; d++;e++;
        }    
    }
    return NULL;
}

void *cons(void *arg) {
    MidiEvent rcvd;
    for (int i = 0; i < 48; ) {
        if (spsc_pop(&rb, &rcvd, sizeof(rcvd))) {
            printf("Received {event:%d,channel:%d,note:%d,velocity:%d,value:%d}\n", 
                    rcvd.type, rcvd.channel,rcvd.note, rcvd.velocity, rcvd.value);
            i++;
        }
    }
    return NULL;
}

int main(void) {
    spsc_init(&rb);
    printf("sizeof size_t = %zu\n", sizeof(size_t));
    printf("sizeof float = %zu\n", sizeof(float));
    printf("sizeof double = %zu\n", sizeof(double));
    MidiEvent testme = {0,1,2,3,69};
    printf("sizeof MidiEvent %zu\n", sizeof(testme));
    printf("testme.value = %d\n", testme.value);
    pthread_t p, c;
    pthread_create(&p, NULL, prod, NULL);
    pthread_create(&c, NULL, cons, NULL);

    pthread_join(p, NULL);
    pthread_join(c, NULL);

    return 0;
}
