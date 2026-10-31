.PHONY: all clean
all: comp tools

comp:
	cd comp && halcompile --compile powerlink.comp

tools:
	make -C tools

test:
	cd tools && ./shm_proto_test

clean:
	rm -f comp/powerlink.so comp/powerlink.c
	make -C tools clean
