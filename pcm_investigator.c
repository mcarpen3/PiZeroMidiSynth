#include <stdio.h>
#include <alsa/asoundlib.h>

char *print_bool(int b)
{
    if (b) return "true";
    return "false";
}
    
int main(void)
{
    unsigned int rrate, period_time = 100000, buffer_time = 500000;
    snd_pcm_sframes_t period_size, buffer_size;
    snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
    unsigned int format_bits = snd_pcm_format_width(format);
    unsigned int maxval = (1 << (format_bits - 1)) - 1;
    int err, dir, bps, phys_bps, big_endian, to_unsigned, is_float, resample = 1;
    const char *device = "plughw:0,0";
    double rate = 44100;
    unsigned int rate2;
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);

    if ((err = snd_pcm_open(&pcm_handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        return 0;
    } 

    err = snd_pcm_hw_params_any(pcm_handle, params);
    if (err < 0) {
        printf("broke config for playback %s\n", snd_strerror(err));
    }

    
    err = snd_pcm_hw_params_set_rate_resample(pcm_handle, params, resample);
    if (err < 0) {
        printf("Resampling setup failed %s\n", snd_strerror(err));
    }
    err = snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        printf("Access type not available for playback %s\n", snd_strerror(err));
    }
    err = snd_pcm_hw_params_set_format(pcm_handle, params, 1);
    if (err < 0) {
        printf("Channels count 1 not available %s\n", snd_strerror(err));
    }
    rrate = rate;
    err = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rrate, 0);
    if (err < 0) {
        printf("Rate %uHz not available %s\n", rate, snd_strerror(err));
    }
    
    err = snd_pcm_hw_params_get_rate(params, &rate2, &dir);
    if (err < 0) {
        printf("get rate error %s\n", snd_strerror(err));
    } else {
        printf("get rate %u dir %i\n", rate2, dir);
    }

    if (rrate != rate) {
        printf("Rate %iHz doesn't match requested %iHz\n", err, rate); 
    }
    err = snd_pcm_hw_params_set_buffer_time_near(pcm_handle, params, &buffer_time, &dir);
    if (err < 0) {
        printf("Unable to set buffer time %u %s\n", buffer_time, snd_strerror(err));
    }
    err = snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
    if (err < 0) {
        printf("Unable to get buffer size %s\n", snd_strerror(err));
    } else {
        printf("Buffer size %lu returned\n", buffer_size);
    }
    err = snd_pcm_hw_params_set_period_time_near(pcm_handle, params, &period_time, &dir);
    if (err < 0) {
        printf("Unable to set period time %u %s\n", period_time, snd_strerror(err));
    } else {
        printf("Period time returned %u\n", period_time);
    }
    err = snd_pcm_hw_params_get_period_size(params, &period_size, &dir);
    if (err < 0) {
        printf("Unable to get period_size %s\n", snd_strerror(err));
    } else {
        printf("Got period_size %i\n", period_size);
    }


    bps = format_bits / 8;  /* bytes per sample */
    phys_bps = snd_pcm_format_physical_width(format) / 8;
    big_endian = snd_pcm_format_big_endian(format) == 1;
    to_unsigned = snd_pcm_format_unsigned(format) == 1;
    is_float = (format == SND_PCM_FORMAT_FLOAT_LE || format == SND_PCM_FORMAT_FLOAT_BE);
    printf("pcm format %d\n  sample width = %d\n  bytes per sample = %d\n  phys_bps = %d\n  big_endian = %s\n  to_unsigned = %s\n  is_float = %s\n", 
            format, format_bits, bps, phys_bps, print_bool(big_endian), print_bool(to_unsigned), print_bool(is_float));
    printf("samples type signed short is %zu bytes per\n", sizeof(signed short));
    snd_pcm_close(pcm_handle);

    return 0;
}
