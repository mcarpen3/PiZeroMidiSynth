SHELL=/bin/bash

CC        := gcc
SRC_DIR   := src
INC_DIR   := include

BUILD_DIR := build
TARGET := seqsynth
SRCS   := $(shell find $(SRC_DIR) -name '*.c')
OBJS   := $(SRCS:%=$(BUILD_DIR)/%.o)
LIBS   := -lmp3lame -lasound -lm -pthread

CFLAGS    := -Wall -Wextra -Wno-unused-variable -Wno-unused-parameter -Wno-unused-function -Iinclude

.PHONY: all clean

$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/$(TARGET) main.c $(OBJS) $(LIBS)

clean:
	rm -rf $(BUILD_DIR)

# Testing lame mp3 decoding and pcm playback
$(BUILD_DIR)/playback: tests/playback.c $(BUILD_DIR)/src/synth_pcm.c.o $(BUILD_DIR)/src/synth_lame.c.o
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/playback tests/playback.c $(BUILD_DIR)/src/synth_pcm.c.o $(BUILD_DIR)/src/synth_lame.c.o -lasound -lmp3lame

# Build rules for object files
$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS)

