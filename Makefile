all:
	gcc -o avs2yuv avs2yuv.c avisynth_c.lib -mno-cygwin -O3 -s -march=i686
debug:
	gcc -o avs2yuv avs2yuv.c avisynth_c.lib -mno-cygwin -O0 -g
