BUILD_DIR=build/

seqsynth: main.c synth.o synth_lame.o synth_pcm.o spsc_ring_buffer.o dlist.o
	gcc -ggdb -Wall -Wextra -Wno-unused-variable -o $(BUILD_DIR)seqsynth main.c \
		$(BUILD_DIR)synth.o $(BUILD_DIR)synth_pcm.o $(BUILD_DIR)synth_lame.o $(BUILD_DIR)spsc_ring_buffer.o $(BUILD_DIR)dlist.o -lasound -pthread -lm -lmp3lame
synth.o: synth.c synth.h
	gcc -ggdb -c synth.c -o $(BUILD_DIR)synth.o
synth_pcm.o: synth_pcm.c synth_pcm.h
	gcc -c synth_pcm.c -o $(BUILD_DIR)synth_pcm.o
synth_lame.o: synth_lame.c synth_lame.h
	gcc -c synth_lame.c -o $(BUILD_DIR)synth_lame.o
spsc_ring_buffer.o: spsc_ring_buffer.c
	gcc -c spsc_ring_buffer.c -o $(BUILD_DIR)spsc_ring_buffer.o
dlist.o: DList/dlist.c DList/dlist.h
	gcc -c DList/dlist.c -o $(BUILD_DIR)dlist.o
clean:
	rm $(BUILD_DIR)*
build_and_run: seqsynth
	build/seqsynth
run: 
	build/seqsynth $(ARGS)
debug:
	gdb build/seqsynth
playback: synth_pcm.o synth_lame.o
	gcc -o build/playback $(BUILD_DIR)synth.o -lasound -lmp3lame -lm
