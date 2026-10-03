seqsynth: main.c synth.o spsc_ring_buffer.o 
	gcc -o seqsynth main.c synth.o spsc_ring_buffer.o -lasound -pthread -lm
synth.o: synth.c synth.h
	gcc -c synth.c
spsc_ring_buffer.o: spsc_ring_buffer.c
	gcc -c spsc_ring_buffer.c 
test_pcm: test_pcm.c
	gcc -Wall -o test_pcm test_pcm.c -lasound -lm
pcm_investigator: pcm_investigator.c
	gcc -o pcm_investigator pcm_investigator.c -lasound
spsc_ring_buf_test: spsc_ring_buf_test.c
	gcc -o ring_buf_test spsc_ring_buf_test.c -pthread -lm
alloca_alsa_swparams_stack: alloca_alsa_swparams_stack.c
	gcc -o alloca alloca_alsa_swparams_stack.c -lasound
clean:
	rm *.o seqsynth
