#include <stdio.h>
#include "./synth_pcm.h"
#include <lame/lame.h>
#include <unistd.h>

#define BUFFER_SZ 1024 
#define PCM_BUFFER_SZ (1024 * 16)
/*
 * NOTE: this is required on 32-bit ARM NEON/SIMD when the buffers don't align to 16-byte boundaries
 * __attribute__((aligned(16)))
 */
int main(int argc, char *argv[]) {
    FILE *mp3_file;
    int rc, sample_count = 0;
    unsigned char mp3_buffer[BUFFER_SZ] __attribute__((aligned(16)));
    short pcm_l[PCM_BUFFER_SZ] __attribute__((aligned(16)));
    short pcm_r[PCM_BUFFER_SZ] __attribute__((aligned(16)));
    synth_pcm_init();
    hip_t dec_gf;
    int sample_idx = 0;
    mp3data_struct mp3_struct = {0};
    int printed_mp3 = 0;

    if ((mp3_file = fopen(argv[1], "rb")) == NULL) {
        fprintf(stderr, "ERROR opening mp3 %s %d\n", argv[1], strerror(rc));
        return 1;
    }

    if ((dec_gf = hip_decode_init()) == NULL) {
        fprintf(stderr, "ERROR init hip decoder\n");
        return 1;
    }

    while((rc = fread(mp3_buffer, sizeof(unsigned char), BUFFER_SZ, mp3_file)) > 0) {
        if (!mp3_struct.header_parsed) {
            rc = hip_decode_headers(dec_gf, mp3_buffer, rc, pcm_l, pcm_r, &mp3_struct);
        } else {
            rc = hip_decode(dec_gf, mp3_buffer, rc, pcm_l, pcm_r);
        }
        //printf("hip_decode: %d\n", rc);
        for (int i = 0; i < rc; ++i) {
            if (sample_idx++ >= synth_pcm_period_size()) {
                sample_idx = 0;
                int written = synth_pcm_write();
                // printf("synth_pcm_write - %d\n", rc);
                if (written== -EAGAIN) {
                    printf("continue\n");
                    continue;
                } 
                if (written < 0) {
                    if (synth_pcm_xrun_recovery(written) < 0) {
                        printf("Write error: %s\n", snd_strerror(written));
                        break;
                    }
                    continue;  /* skip one period */
                }

            }
            synth_pcm_sample(sample_idx, pcm_l[i]);
        }
    }
    int flushed_samples = 0;
    flushed_samples = hip_decode(dec_gf, NULL, 0, pcm_l, pcm_r);

    if (flushed_samples > 0) {
        for (int i = 0; i < flushed_samples; ++i) {
            synth_pcm_sample(sample_idx, pcm_l[i]);
            if (sample_idx++ >= synth_pcm_period_size()) {
                sample_idx = 0;
                int write_rc = synth_pcm_write();
                if (write_rc < 0) {
                    synth_pcm_xrun_recovery(write_rc);
                }
            }
        }
    }
    if (sample_idx > 0) {
        while(sample_idx < synth_pcm_period_size()) {
            synth_pcm_sample(sample_idx++, 0);
        }
        synth_pcm_write();
    }
    printf("read all bytes i think %d\n", rc);
    printf("hip_decode_exit BEGIN\n");
    hip_decode_exit(dec_gf);
    printf("hip_decode_exit END\n");
    synth_pcm_shutdown();
    printf("synth_pcm_shutdown END\n");
    fclose(mp3_file);
    return 0;
 }
