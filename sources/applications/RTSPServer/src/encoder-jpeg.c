/*
 * Copyright (C) 2004 Nathan Lutchansky <lutchann@litech.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <linux/videodev.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <inttypes.h>

#include <event.h>
#include <log.h>
#include <frame.h>
#include <stream.h>
#include <conf_parse.h>

#include "utils.h"
#include "V4L.h"
#include "JpegEnc.h"

struct jpeg_encoder {
	struct stream *output;
	int format;
	struct frame_exchanger *ex;
	pthread_t thread;
	int running;
	int bitrate;
	uint32_t u32VinWidth;
	uint32_t u32VinHeight;
	uint32_t u32JpegWidth;
	uint32_t u32JpegHeight;
};

#define DEF_FRAME_RATE			30

//#define DEF_VIN_WIDTH			640
//#define DEF_VIN_HEIGHT			480

//#define DEF_JPEG_ENC_WIDTH		640
//#define DEF_JPEG_ENC_HEIGHT		480



static void *jpeg_loop(void *d)
{
	struct jpeg_encoder *en = (struct jpeg_encoder *)d;
	S_NM_VIDEOCTX sVideoSrcCtx;
	S_NM_VIDEOCTX sVideoDstCtx;	
	uint32_t u32FrameSize = 0;
	struct frame *psJpegFrame = NULL;;
	int i32MaxFrameSize = get_max_frame_size();

	uint64_t u64StartBitRateTime = 0;
	uint32_t u32SendDataBytes = 0;
	double dCurBitRate = 0;
	uint32_t u32UpdateFrameTime;
	double dMaybeFR;
	uint32_t u32SendFrame = 0;
	uint64_t u64CurTime;
	uint32_t u32DesireBitRate;
	uint32_t u32DesireFrameRate;

	uint64_t u64NextFrameTime;

	u32DesireBitRate = en->bitrate;
	u32DesireFrameRate =  DEF_FRAME_RATE;

	//Setup video-in context
	memset(&sVideoSrcCtx, 0x00, sizeof(S_NM_VIDEOCTX));

	sVideoSrcCtx.u32Width = en->u32VinWidth;
	sVideoSrcCtx.u32Height = en->u32VinHeight;
	sVideoSrcCtx.u32StrideW = sVideoSrcCtx.u32Width;
	sVideoSrcCtx.u32StrideH = sVideoSrcCtx.u32Height ;
	sVideoSrcCtx.eColorType = eNM_COLOR_YUV420P;

	//Setup JPEG encode context
	memset(&sVideoDstCtx, 0x00, sizeof(S_NM_VIDEOCTX));

	sVideoDstCtx.u32Width = en->u32JpegWidth;
	sVideoDstCtx.u32Height = en->u32JpegHeight;
	sVideoDstCtx.u32StrideW = sVideoDstCtx.u32Width;
	sVideoDstCtx.u32StrideH = sVideoDstCtx.u32Height ;
	sVideoDstCtx.eColorType = eNM_COLOR_YUV420P;
	sVideoDstCtx.u32FrameRate = DEF_FRAME_RATE;
	sVideoDstCtx.u32BitRate = en->bitrate;
		
	//Open video-in device
	ERRCODE eVinErr;
	eVinErr = InitV4LDevice(&sVideoSrcCtx);
	if(eVinErr != ERR_V4L_SUCCESS){
		printf("Open video-in device failed \n");
		return 0;
	}

	//Open JPEG device
	int32_t i32JpegRet;
	i32JpegRet = InitJpegDevice();
	if(i32JpegRet <= 0){
		printf("Open JPEG encoder failed \n");
		return 0;
	}

	u64StartBitRateTime = utils_get_ms_time();
	u64NextFrameTime = utils_get_ms_time();
	u32UpdateFrameTime = 0; //= 1000 / u32DesireFrameRate;
	u32SendFrame = 0;

	//Start video-in capture
	StartV4LCapture();

	for (;;) {
		if (!en->running) {
			usleep(10000);
			continue;
		}

		if (u64NextFrameTime > (utils_get_ms_time() + 10)) {
			usleep(10000);
			continue;
		}


		//Read raw picture
		eVinErr = ReadV4LPicture(&sVideoSrcCtx);
		if(eVinErr != ERR_V4L_SUCCESS){
			continue;
		}
		u64CurTime = utils_get_ms_time();

		//Encode JPEG
		i32JpegRet = JpegEnc(&sVideoSrcCtx, &sVideoDstCtx);

		while((utils_get_ms_time() == u64CurTime));

		//Trig next frame
		TriggerV4LNextFrame();

		i32JpegRet = WaitJpegEncDone(&sVideoDstCtx);

		if(i32JpegRet <= 0){
			continue;
		}

		u32FrameSize = sVideoDstCtx.u32DataSize;
		if (u32FrameSize > i32MaxFrameSize) {
			spook_log(SL_WARN, "jpeg: encode size large than frame size \n");
			usleep(10000);
			continue;
		}

		psJpegFrame = new_frame();

		if (psJpegFrame == NULL) {
			usleep(10000);
			continue;
		}

		memcpy(psJpegFrame->d, sVideoDstCtx.pDataVAddr, u32FrameSize);

		psJpegFrame->format = FORMAT_JPEG;
		psJpegFrame->width = sVideoDstCtx.u32Width;
		psJpegFrame->height = sVideoDstCtx.u32Height;
		psJpegFrame->key = 1;
		psJpegFrame->length = u32FrameSize;

		deliver_frame(en->ex, psJpegFrame);

		u32SendFrame ++;
		u32SendDataBytes += u32FrameSize;
		if (u32DesireBitRate) {
			dMaybeFR = (u32DesireBitRate / 8) / u32FrameSize;

			if(dMaybeFR==0)	dMaybeFR = 1;

			if (dMaybeFR > u32DesireFrameRate)
				dMaybeFR = u32DesireFrameRate;

			u32UpdateFrameTime = 1000 / (unsigned int)dMaybeFR;
			u32UpdateFrameTime = (u32UpdateFrameTime / 10) * 10;

			if (u32UpdateFrameTime > 10)
				u32UpdateFrameTime -= 10;
		}

		if ((u64CurTime - u64StartBitRateTime) > 1000) {
			dCurBitRate = (double)(u32SendDataBytes * 8) / ((double)(u64CurTime - u64StartBitRateTime) / 1000);
			dMaybeFR = (double)(u32SendFrame) / ((double)(u64CurTime - u64StartBitRateTime) / 1000);
			printf("RTSP send JPEG bit rate %f bps, frame rate %f fps\n", dCurBitRate, dMaybeFR);
			//			printf("Current RTSP frame rate %f fps\n", dMaybeFR);
			u64StartBitRateTime = utils_get_ms_time();
			u32SendDataBytes = 0;
			u32SendFrame = 0;

			dMaybeFR = 0;
		}

		u64NextFrameTime = u64CurTime + u32UpdateFrameTime;
	}

	FinializeV4LDevice();
	FinializeJpegDevice();

	return NULL;
}


static void get_framerate(struct stream *s, int *fincr, int *fbase)
{
	*fincr = 1;
	*fbase = DEF_FRAME_RATE;
}

static void set_running(struct stream *s, int running)
{
	struct jpeg_encoder *en = (struct jpeg_encoder *)s->private;

	en->running = running;
}

static void get_jpeg_enc_pipe_num(struct stream *s, int *pipe_num)
{
	*pipe_num = 0;
}

/************************ CONFIGURATION DIRECTIVES ************************/

static void *start_block(void)
{
	struct jpeg_encoder *en;

	en = (struct jpeg_encoder *)malloc(sizeof(struct jpeg_encoder));
	memset(en, 0x00, sizeof(struct jpeg_encoder));
	en->output = NULL;

	return en;
}

static int end_block(void *d)
{
	struct jpeg_encoder *en = (struct jpeg_encoder *)d;

	if (! en->output) {
		spook_log(SL_ERR, "jpeg: missing output stream name");
		return -1;
	}

	en->ex = new_exchanger(8, deliver_frame_to_stream, en->output);

	//use push callback instead of import
	pthread_create( &en->thread, NULL, jpeg_loop, en );

	return 0;
}

static int set_pipe_num(int num_tokens, struct token *tokens, void *d)
{
	return 0;
}

static int set_vin_width(int num_tokens, struct token *tokens, void *d)
{
	struct jpeg_encoder *en = (struct jpeg_encoder *)d;

	en->u32VinWidth = tokens[1].v.num;

	return 0;
}

static int set_vin_height(int num_tokens, struct token *tokens, void *d)
{
	struct jpeg_encoder *en = (struct jpeg_encoder *)d;

	en->u32VinHeight = tokens[1].v.num;

	return 0;
}

static int set_jpeg_width(int num_tokens, struct token *tokens, void *d)
{
	struct jpeg_encoder *en = (struct jpeg_encoder *)d;

	en->u32JpegWidth = tokens[1].v.num;

	return 0;
}

static int set_jpeg_height(int num_tokens, struct token *tokens, void *d)
{
	struct jpeg_encoder *en = (struct jpeg_encoder *)d;

	en->u32JpegHeight = tokens[1].v.num;

	return 0;
}

static int set_bitrate_num(int num_tokens, struct token *tokens, void *d)
{
	struct jpeg_encoder *en = (struct jpeg_encoder *)d;

	en->bitrate = tokens[1].v.num;

	return 0;
}


static int set_output(int num_tokens, struct token *tokens, void *d)
{
	struct jpeg_encoder *en = (struct jpeg_encoder *)d;

	en->output = new_stream(tokens[1].v.str, FORMAT_JPEG, en);

	if (! en->output) {
		spook_log(SL_ERR, "jpeg: unable to create stream \"%s\"",
				  tokens[1].v.str);
		return -1;
	}

	en->output->get_framerate = get_framerate;
	en->output->set_running = set_running;
	en->output->get_enc_pipe_num = get_jpeg_enc_pipe_num;
	return 0;
}


static struct statement config_statements[] = {
	/* directive name, process function, min args, max args, arg types */
	{ "output", set_output, 1, 1, { TOKEN_STR } },
	{ "bitrate", set_bitrate_num, 1, 1, { TOKEN_NUM } },
	{ "pipe", set_pipe_num, 1, 1, { TOKEN_NUM } },
	{ "vin_width", set_vin_width, 1, 1, { TOKEN_NUM } },
	{ "vin_height", set_vin_height, 1, 1, { TOKEN_NUM } },
	{ "jpeg_width", set_jpeg_width, 1, 1, { TOKEN_NUM } },
	{ "jpeg_height", set_jpeg_height, 1, 1, { TOKEN_NUM } },
	/* empty terminator -- do not remove */
	{ NULL, NULL, 0, 0, {} }
};


int jpeg_init(void)
{
	register_config_context("encoder", "jpeg", start_block, end_block,
							config_statements);
	return 0;
}


