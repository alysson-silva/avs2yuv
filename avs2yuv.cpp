// Avs2YUV by Loren Merritt

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>
#include "internal.h"

#define MY_VERSION "Avs2YUV 0.20"

int __cdecl main(int argc, const char* argv[])
{
	const char* infile = NULL;
	const char* outfile = NULL;
	FILE* yuv_out = NULL;
	int verbose = 0;
	int usage = 0;
	int seek = 0;
	int end = 0;
	int i, frm = -1;
	
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
			const char *dot = strrchr(infile, '.');
			if(!dot || strcmp(".avs", dot))
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
		"Output format is yuv4mpeg, as used by MPlayer and mjpegtools\n"
		);
		return 2;
	}

	try {
		HMODULE avsdll = LoadLibrary("avisynth.dll");
		if(!avsdll)
			{fprintf(stderr, "failed to load avisynth.dll\n"); return 2;}
		IScriptEnvironment* (* CreateScriptEnvironment)(int version)
			= (IScriptEnvironment*(*)(int)) GetProcAddress(avsdll, "CreateScriptEnvironment");
		if(!CreateScriptEnvironment)
			{fprintf(stderr, "failed to load CreateScriptEnvironment()\n"); return 1;}
	
		IScriptEnvironment* env = CreateScriptEnvironment(AVISYNTH_INTERFACE_VERSION);
		AVSValue arg(infile);
		AVSValue res = env->Invoke("Import", AVSValue(&arg, 1));
		if(!res.IsClip())
			{fprintf(stderr, "Error: '%s' didn't return a video clip.\n", infile); return 1;}
		PClip clip = res.AsClip();
		VideoInfo inf = clip->GetVideoInfo();
	
		fprintf(stderr, "%s: %dx%d, ", infile, inf.width, inf.height);
		if(inf.fps_denominator == 1)
			fprintf(stderr, "%d fps, ", inf.fps_numerator);
		else
			fprintf(stderr, "%d/%d fps, ", inf.fps_numerator, inf.fps_denominator);
		fprintf(stderr, "%d frames\n", inf.num_frames);

		if(!inf.IsYV12()) {
			fprintf(stderr, "converting %s -> YV12\n", inf.IsYUY2() ? "YUY2" : inf.IsRGB() ? "RGB" : "?");
			res = env->Invoke("converttoyv12", AVSValue(&res, 1));
			clip = res.AsClip();
			inf = clip->GetVideoInfo();
		}
		if(!inf.IsYV12())
			{fprintf(stderr, "Couldn't convert input to YV12\n"); return 1;}
		if(inf.IsFieldBased())
			{fprintf(stderr, "Needs progressive input\n"); return 1;}
	
		if(outfile) {
			if(!strcmp(outfile, "-")) {
				_setmode(_fileno(stdout), O_BINARY);
				yuv_out = stdout;
			} else {
				yuv_out = fopen(outfile, "wb");
				if(!yuv_out)
					{fprintf(stderr, "fopen(\"%s\") failed", outfile); return 1;}
			}
			fprintf(yuv_out, "YUV4MPEG2 W%d H%d F%ld:%ld Ip A0:0\n",
				inf.width, inf.height, inf.fps_numerator, inf.fps_denominator);
			fflush(yuv_out);
		}
		
		if(end <= seek || end > inf.num_frames)
			end = inf.num_frames;
		for(frm = seek; frm < end; ++frm) {
			PVideoFrame f = clip->GetFrame(frm, env);
	
			if(yuv_out) {
				int p;
				static const int planes[] = {PLANAR_Y, PLANAR_U, PLANAR_V};
				fwrite("FRAME\n", 1, 6, yuv_out);
				for(p=0; p<3; p++) {
					int wrote = 0;
					int w = inf.width  >> (p ? 1 : 0);
					int h = inf.height >> (p ? 1 : 0);
					int pitch = f->GetPitch(planes[p]);
					const BYTE* data = f->GetReadPtr(planes[p]);
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
			
			if(verbose)
				fprintf(stderr, "%d\n", frm);
		}
	} catch(AvisynthError err) {
		if(frm >= 0)
			fprintf(stderr, "\nAvisynth error at frame %d:\n%s\n", frm, err.msg);
		else
			fprintf(stderr, "\nAvisynth error:\n%s\n", err.msg);
		return 1;
	}

	if(yuv_out)
		fclose(yuv_out);
	return 0;
}
