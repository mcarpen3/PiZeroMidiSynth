#ifndef SYNTH_LAME_H
#define SYNTH_LAME_H
#include <lame/lame.h>
#include <stdlib.h>

lame_global_flags *lame_flags;
static int mp3_buf_sz;
static unsigned char *mp3_buf;
static long synth_lame_period_size;

int synth_lame_codec_configure(long period_size, int channels);
int synth_lame_encode_buffer(short *buffer);
unsigned char *synth_lame_mp3_buffer();
void synth_lame_close();
int synth_lame_recording_flush();

#endif // SYNTH_LAME_H

