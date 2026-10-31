#include <stdio.h>
#include <lame/lame.h>
#include <alsa/libasound.h>
#include "./synth.h"

#define BUFFER_SZ 1024 
#define PCM_BUFFER_SZ 1024

int main(int argc, char *argv[]) {
    FILE *mp3_file;
    int rc;
    unsigned char mp3_buffer[BUFFER_SZ];
    short pcm_l[PCM_BUFFER_SZ];
    lame_codec_configure();
    init_pcm();

    if ((rc = fopen(argv[1], "rb")) < 0) {
        fprintf(stderr, "ERROR opening mp3 %s %d\n", argv[1], strerror(rc));
        return 1;
    }

    if ((hip_t dec_gf = hip_decode_init()) == NULL) {
        fprintf(stderr, "ERROR init hip decoder\n");
        return 1;
    }

    while((rc = fread(mp3_buffer, sizeof(unsigned char), BUFFER_SZ)) > 0) {
        while ((rc = hip_decode(dec_gf, mp3_buffer, BUFFER_SZ, pcm_l, NULL)) > 0) {
            snd_pcm_writei(pcm_handle, pcm_l, rc);
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
    }
    hip_decode_exit(dec_gf);
    return snd_pcm_close(pcm_handle);
 }
