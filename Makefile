all:
	gcc -O3 -march=i686 avs2yuv.c avisynth_c.lib -o avs2yuv -mno-cygwin

