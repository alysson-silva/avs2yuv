all:
	gcc -O3 -s -march=i686 avs2yuv.c avisynth_c.lib -o avs2yuv -mno-cygwin
debug:
	gcc -O0 -g avs2yuv.c avisynth_c.lib -o avs2yuv -mno-cygwin
