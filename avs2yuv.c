// Avs2YUV by Loren Merritt

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "avisynth_c.h"

#define MY_VERSION "Avs2YUV 0.12"

int main(int argc, const char* argv[])
{
	const char* infile = NULL;
	const char* outfile = NULL;
	int verbose = 0;
	int usage = 0;
	int seek = 0;
	int end = 0;
	int i;
	
	for(i=1; i<argc; i++) {
		if(argv[i][0] == '-' && argv[i][1] != 0) {
			if(argv[i][1] == 'v')
				verbose = 1;
			else if(argv[i][1] == 'h')
				usage = 1;
			else if(!strcmp(argv[i]+1, "seek")) {
				if(i > argc-2)
					{fprintf(stderr, "-seek needs an argument\n"); return 2;}
				seek = atoi(argv[i+1]);
				i++;
			} else if(!strcmp(argv[i]+1, "frames")) {
				if(i > argc-2)
					{fprintf(stderr, "-frames needs an argument\n"); return 2;}
				end = seek + atoi(argv[i+1]);
				i++;
			} else
				{fprintf(stderr, "no such option: %s\n", argv[i]); return 2;}
		} else if(!infile) {
			infile = argv[i];
			if(!strrchr(infile, '.') || strcmp(".avs", strrchr(infile, '.')) != 0)
				fprintf(stderr, "infile (%s) doesn't look like an avisynth script\n", infile);
		} else if(!outfile)
			outfile = argv[i];
		else
			usage = 1;
	}
	if(seek < 0)
		usage = 1;
	if(usage || !infile || (!outfile && !verbose)) {
		fprintf(stderr, MY_VERSION "\n"
		"Usage: avs2yuv [options] in.avs out.yuv\n"
		"-v\tprint the frame number after processing each frame\n"
		"-seek\tseek to the given frame number\n"
		"-frames\tstop after processing this many frames\n"
		"The outfile may be \"-\", meaning stdout.\n"
		"Output is in yuv4mpeg, as used by MPlayer and mjpegtools\n"
		);
		return 2;
	}
	fprintf(stderr, "%s\n", infile);
	
	AVS_ScriptEnvironment* env = 
		avs_create_script_environment(AVISYNTH_INTERFACE_VERSION);
	AVS_Value afile = avs_new_value_string(infile);
	AVS_Value args = avs_new_value_array(&afile, 1);
	AVS_Value res = avs_invoke(env, "import", args, 0);
	AVS_Clip* clip = avs_take_clip(res, env);
	const AVS_VideoInfo* inf = avs_get_video_info(clip);

	if(!avs_is_yv12(inf)) {
		fprintf(stderr, "Converting to YV12\n");
		avs_release_value(args);
		args = avs_new_value_array(&res, 1);
		res = avs_invoke(env, "converttoyv12", args, 0);
		clip = avs_take_clip(res, env);
		inf = avs_get_video_info(clip);
	}
	avs_release_value(afile);
	avs_release_value(args);
	avs_release_value(res);
	if(!avs_is_yv12(inf))
		{fprintf(stderr, "Couldn't convert input to YV12\n"); return 1;}
	if(avs_is_field_based(inf))
		{fprintf(stderr, "Needs progressive input\n"); return 1;}

	FILE* yuv_out = NULL;
	if(outfile) {
		if(0==strcmp(outfile, "-"))
			yuv_out = fdopen(STDOUT_FILENO, "wb");
		else {
			yuv_out = fopen(outfile, "wb");
			if(!yuv_out)
				{fprintf(stderr, "fopen(\"%s\") failed", outfile); return 1;}
		}
		fprintf(yuv_out, "YUV4MPEG2 W%d H%d F%ld:%ld Ip A0:0\n",
			inf->width, inf->height, inf->fps_numerator, inf->fps_denominator);
		fflush(yuv_out);
	}
	
	fprintf(stderr, "total frames: %d\n", inf->num_frames);
	if(end <= seek || end > inf->num_frames)
		end = inf->num_frames;
	for(i = seek; i < end; ++i) {
		AVS_VideoFrame* f = avs_get_frame(clip, i);

		if(yuv_out) {
			int p;
			static const int planes[] = {AVS_PLANAR_Y, AVS_PLANAR_U, AVS_PLANAR_V};
			fwrite("FRAME\n", 1, 6, yuv_out);
			for(p=0; p<3; p++) {
				int wrote = 0;
				int w = inf->width  >> (p ? 1 : 0);
				int h = inf->height >> (p ? 1 : 0);
				int pitch = avs_get_pitch_p(f, planes[p]);
				const BYTE* data = avs_get_read_ptr_p(f, planes[p]);
				int y;
				for(y=0; y<h; y++) {
					wrote += fwrite(data, 1, w, yuv_out);
					data += pitch;
				}
				if(wrote != w*h) {
					fprintf(stderr, "Output error: wrote only %d of %d bytes\n", wrote, w*h);
					return 1;
				}
			}
		}
		
		avs_release_frame(f);
		if(verbose)
			fprintf(stderr, "%d\n", i);
	}

	if(yuv_out)
		fclose(yuv_out);
	avs_release_clip(clip);
	return 0;
}
