BUILD_DIR=build/


seqsynth: main.c synth.o spsc_ring_buffer.o 
	gcc -ggdb -Wall -Wextra -Wno-unused-variable -o $(BUILD_DIR)seqsynth main.c \
		$(BUILD_DIR)synth.o $(BUILD_DIR)spsc_ring_buffer.o -lasound -pthread -lm
synth.o: synth.c synth.h
	gcc -ggdb -c synth.c -o $(BUILD_DIR)synth.o
spsc_ring_buffer.o: spsc_ring_buffer.c
	gcc -c spsc_ring_buffer.c -o $(BUILD_DIR)spsc_ring_buffer.o
test_pcm: test_pcm.c
	gcc -Wall -o $(BUILD_DIR)test_pcm test_pcm.c -lasound -lm
pcm_investigator: pcm_investigator.c
	gcc -o $(BUILD_DIR)pcm_investigator pcm_investigator.c -lasound
spsc_ring_buf_test: spsc_ring_buf_test.c
	gcc -o $(BUILD_DIR)ring_buf_test spsc_ring_buf_test.c -pthread -lm
alloca_alsa_swparams_stack: alloca_alsa_swparams_stack.c
	gcc -o $(BUILD_DIR)alloca alloca_alsa_swparams_stack.c -lasound
test_int_sin: test_int_sin.c
	gcc -o $(BUILD_DIR)test_int_sin test_int_sin.c -lm
clean:
	rm $(BUILD_DIR)*
build_and_run: seqsynth
	build/seqsynth
run:
	build/seqsynth $(ARGS)
debug:
	gdb build/seqsynth
run_test_pcm:
	build/test_pcm
run_test_int_sin:
	build/test_int_sin
