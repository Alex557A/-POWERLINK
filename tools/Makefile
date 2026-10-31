.PHONY: all clean
all: plk_sim shm_proto_test

plk_sim: plk_sim.c ../comp/powerlink_shm.h
	gcc -O2 -Wall -o $@ $<

shm_proto_test: shm_proto_test.c ../comp/powerlink_shm.h
	gcc -O2 -Wall -o $@ $<

clean:
	rm -f plk_sim shm_proto_test
