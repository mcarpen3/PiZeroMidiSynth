BUILD_DIR=build/

seqsynth: main.c synth.o spsc_ring_buffer.o dlist.o
	gcc -ggdb -Wall -Wextra -Wno-unused-variable -o $(BUILD_DIR)seqsynth main.c \
		$(BUILD_DIR)synth.o $(BUILD_DIR)spsc_ring_buffer.o $(BUILD_DIR)dlist.o -lasound -pthread -lm
synth.o: synth.c synth.h
	gcc -ggdb -c synth.c -o $(BUILD_DIR)synth.o
spsc_ring_buffer.o: spsc_ring_buffer.c
	gcc -c spsc_ring_buffer.c -o $(BUILD_DIR)spsc_ring_buffer.o
dlist.o: DList/dlist.c DList/dlist.h
	gcc -c DList/dlist.c -o $(BUILD_DIR)dlist.o
clean:
	rm $(BUILD_DIR)*
build_and_run: seqsynth
	build/seqsynth
run: seqsynth
	build/seqsynth $(ARGS)
debug:
	gdb build/seqsynth

