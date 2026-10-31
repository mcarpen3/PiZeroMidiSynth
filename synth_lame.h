#ifndef SYNTH_LAME_H
#define SYNTH_LAME_H
#include <lame/lame.h>
#include <stdlib.h>

lame_global_flags *lame_flags;
void synth_lame_recording_end();
static FILE *out_file;
static const char *out_filename = "./recording.mp3";
static int mp3_buf_sz;
static unsigned char *mp3_buf;
static long synth_lame_period_size;

int synth_lame_codec_configure(long period_size, int channels);
int synth_lame_encode_buffer(short *buffer);

#endif // SYNTH_LAME_H

