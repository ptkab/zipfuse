all: zipfuse.c
		gcc -Wall -o zipfuse zipfuse.c `pkg-config fuse --cflags --libs` -lzip -g

clean: zipfuse
	rm -rf zipfuse
