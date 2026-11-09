SHELL=/bin/bash

CC        := gcc
SRC_DIR   := src
INC_DIR   := include

BUILD_DIR := build
TARGET := seqsynth
SRCS   := $(shell find $(SRC_DIR) -name '*.c')
OBJS   := $(SRCS:%=$(BUILD_DIR)/%.o)
LIBS   := -lmp3lame -lasound -lm -pthread

CFLAGS    := -Wall -Wextra -Iinclude

.PHONY: all clean

$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/$(TARGET) main.c $(OBJS) $(LIBS)

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS)

clean:
	rm -rf $(BUILD_DIR)

playback: $(BUILD_DIR)/synth_pcm.o $(BUILD_DIR)/synth_lame.o
	gcc -o $(BIN_DIR)/playback $(BUILD_DIR)/synth_pcm.o $(BUILD_DIR)/synth_lame.o playback.c -lasound -lmp3lame

