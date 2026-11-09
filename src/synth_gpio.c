#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/gpio.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <poll.h>
#include "./synth_gpio.h"

static double get_elapsed_time(struct timespec start, struct timespec end) {
    return (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;
}

void handle_sigint(int sig) {
    stop = 1;
}

void synth_gpio_init() {
    atomic_init(&record, 0);
    atomic_init(&recording, 0);
    previous_recording_state = false;
}

int handle_gpio_events() {
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    struct timespec start_press_ns, end_press_ns;
    struct pollfd pfd;
    int rc;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("ERROR registering signal handler");
        return 1;
    }
    gpio_profile gpios[] = {
        {.type = GPIO_PROFILE_INPUT, .num = 27, .val = 0, .fd = -1},
        {.type = GPIO_PROFILE_OUTPUT, .num = 24, .val = 0, .fd = -1},
        {.type = GPIO_PROFILE_OUTPUT, .num = 23, .val = 0, .fd = -1},
    };
    
    // Try open the default raspberry pi gpio chip 0
    int line_fd; 
    if ((line_fd = open("/dev/gpiochip0", O_RDONLY | O_CLOEXEC)) < 0) {
        perror("FAIL: open gpiochip0");
        return 1;
    }
    
    
    // Get chip info
    struct gpiochip_info chip_info = {0};
    if (ioctl(line_fd, GPIO_GET_CHIPINFO_IOCTL, &chip_info) < 0) {
        perror("FAIL: get chip info");
        return 1;
    }
    
    printf("GPIO CHIP INFO name: %s, label: %s, lines: %d\n", chip_info.name, chip_info.label, chip_info.lines);

    for (int i = 0; i < (int)(sizeof(gpios) / sizeof(gpio_profile)); ++i) {
        struct gpio_v2_line_request request = {0};
        request.offsets[0] = gpios[i].num;
        request.num_lines = 1;
        if (gpios[i].type == GPIO_PROFILE_INPUT) {
            request.config.flags = GPIO_V2_LINE_FLAG_INPUT 
                | GPIO_V2_LINE_FLAG_BIAS_PULL_UP 
                | GPIO_V2_LINE_FLAG_EDGE_RISING
                | GPIO_V2_LINE_FLAG_EDGE_FALLING;
        } else {
            request.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
        }

        strcpy(request.consumer, "iqaudio_synth");
        
        if (ioctl(line_fd, GPIO_V2_GET_LINE_IOCTL, &request) < 0) {
            perror("FAIL ioctl get line");
            close(line_fd);
            return 1;
        } else {
            gpios[i].fd = request.fd;
            printf("INFO gpio %d fd = %d\n", gpios[i].num, gpios[i].fd);
        }
    }
    
    struct gpio_v2_line_event event;
    struct gpio_v2_line_values out_values = {};
    pfd.fd = gpios[0].fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    while(stop == 0) {
        rc = poll(&pfd, 1, 800);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                perror("poll failed");
            }
        }
        if (pfd.revents & POLLIN) {
            ssize_t bytes_read = read(gpios[0].fd, &event, sizeof(event));
            if (bytes_read == sizeof(event)) {
                printf("Event detected! Timestamp %llu ns, ID: %u\n", event.timestamp_ns, event.id);
                if (event.id == GPIO_V2_LINE_EVENT_RISING_EDGE) {
                    printf("RECORD_RELEASED\n");
                    clock_gettime(CLOCK_MONOTONIC, &end_press_ns);
                    if (start_press_ns.tv_sec > 0 && get_elapsed_time(start_press_ns, end_press_ns) > 2.) {
                        atomic_store_explicit(&record, !record, memory_order_release);
                        if (verbose)
                            printf("INFO [synth gpio] record pressed\n");
                    }
                } else if (event.id == GPIO_V2_LINE_EVENT_FALLING_EDGE) {
                    printf("RECORD_PRESSED\n");
                    clock_gettime(CLOCK_MONOTONIC, &start_press_ns);
                }
                if (ioctl(gpios[1].fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &out_values) < 0) {
                    fprintf(stderr, "ERROR write %lld to gpio %d - %s\n", out_values.bits, gpios[1].num, strerror(errno));
                    break;
                }
            } else if (errno == EINTR) {
                printf("FAILED to read GPIO event due to INTERRUPT signal %d\n", bytes_read);
                continue;
            } else {
                perror("ERROR fatal");
            }
        }
    }

    for (int i = 0; i < (int)(sizeof(gpios) / sizeof(gpio_profile)); ++i) {
        close(gpios[i].fd);
        printf("INFO: closed gpio %d fd\n", gpios[i].num);
    }
    close(line_fd);
    printf("INFO: closed gpio line\n");
    return 0;
}
