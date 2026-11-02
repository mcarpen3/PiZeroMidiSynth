#include "./synth_lame.h"

int synth_lame_codec_configure(long period_size, int channels) {
    int rc;
    printf("LAME codec configure period_size: %ld, channels: %d\n", period_size, channels);
    if ((lame_flags = lame_init()) == NULL) {
        fprintf(stderr, "ERROR: lame_init failed\n");
        return -1;
    }

    if ((rc = lame_set_num_channels(lame_flags, channels)) != 0) {
        fprintf(stderr, "ERROR: lame_set_num_channels to %d failed\n", channels);
        return rc; 
    }
    if ((rc = lame_init_params(lame_flags)) == -1) {
        fprintf(stderr, "ERROR: lame_init_params %d\n", rc);
        return rc;
    }
    printf("INFO: lame_codec_configure END\n");
    printf("LAME CFG:\n");
    synth_lame_period_size = period_size;
    lame_print_config(lame_flags);
    mp3_buf_sz = (int)(1.25 * period_size) + 7200; // recommended buffer size per lame
    mp3_buf = malloc(mp3_buf_sz);

    return 0;
}

int synth_lame_recording_flush() {
    return lame_encode_flush(lame_flags, mp3_buf, mp3_buf_sz);
}

void synth_lame_close() {
    int rc;
    if ((rc = lame_close(lame_flags)) != 0) {
        fprintf(stderr, "ERROR: synth_lame_close %d\n", rc);
    }

    free(mp3_buf);
    printf("INFO: synth_lame_close COMPLETE\n");
    return;
}

int synth_lame_encode_buffer(short *buffer) {
    return lame_encode_buffer (lame_flags, buffer, NULL, synth_lame_period_size, mp3_buf, mp3_buf_sz);
}

unsigned char *synth_lame_mp3_buffer() {
    return mp3_buf;
}
