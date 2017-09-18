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

#include "Digit_8_16.h"
#include "TextOverlap.h"	//For overlapping
#define DEF_MAX_OVERLAP_TEXT			32
#define DEF_FRAME_RATE			30

struct jpeg_encoder {
	struct stream *output;
	int format;
	struct frame_exchanger *ex;
	pthread_t thread;
	int running;
	int bitrate;
	
	uint32_t u32VinPlannerWidth;
	uint32_t u32VinPlannerHeight;
	uint32_t u32VinPacketWidth;
	uint32_t u32VinPacketHeight;
	
	uint32_t u32JpegWidth;
	uint32_t u32JpegHeight;
	uint32_t u32JpegPacketPipe;
	uint32_t u32Overlapping;
	
};

void OverlapText(
	H_TEXT_OVERLAP hTextOverlap,
	S_NM_VIDEOCTX *psNMVideoCtx,
	uint8_t *pu8BGImgAddr,
	char *pchOverlapStr
)
{
	uint32_t u32StrLen = strlen(pchOverlapStr);
	int32_t i;
	uint8_t *apu8TextAddr[DEF_MAX_OVERLAP_TEXT];
	uint32_t u32BGWidth;
	uint32_t u32BGHeight;

	E_IMGPROC_COLOR_FORMAT eColorFmt = eIMGPROC_YUV422;

	switch ( psNMVideoCtx->eColorType )
	{
		case eNM_COLOR_YUV420P:
			eColorFmt = eIMGPROC_YUV420P;
		break;
		
		case eNM_COLOR_YUV422:
			eColorFmt = eIMGPROC_YUV422;
		break;
		
		default:
			printf("Unsupported\n!");
			return;
	}

	// Code book
	for (i = 0 ; i < u32StrLen; i++) {
		switch (pchOverlapStr[i]) {
		case '0':
			apu8TextAddr[i] = (uint8_t *)s_achPic_8_16_D0;
			break;

		case '1':
			apu8TextAddr[i] = (uint8_t *)s_achPic_8_16_D1;
			break;

		case '2':
			apu8TextAddr[i] = (uint8_t *)s_achPic_8_16_D2;
			break;

		case '3':
			apu8TextAddr[i] = (uint8_t *)s_achPic_8_16_D3;
			break;

		case '4':
			apu8TextAddr[i] = (uint8_t *)s_achPic_8_16_D4;
			break;

		case '5':
			apu8TextAddr[i] = (uint8_t *)s_achPic_8_16_D5;
			break;

		case '6':
			apu8TextAddr[i] = (uint8_t *)s_achPic_8_16_D6;
			break;

		case '7':
			apu8TextAddr[i] = (uint8_t *)s_achPic_8_16_D7;
			break;

		case '8':
			apu8TextAddr[i] = (uint8_t *)s_achPic_8_16_D8;
			break;

		case '9':
			apu8TextAddr[i] = (uint8_t *)s_achPic_8_16_D9;
			break;

		case ':':
			apu8TextAddr[i] = (uint8_t *)s_achPic_8_16_Colon;
			break;

		case '/':
			apu8TextAddr[i] = (uint8_t *)s_achPic_8_16_Slash;
			break;

		default:
			apu8TextAddr[i] = (uint8_t *)s_achPic_8_16_Blank;
			break;
		}
	}

	u32BGWidth = psNMVideoCtx->u32Width;
	u32BGHeight = psNMVideoCtx->u32Height;

	TextOverlap_Overlap(hTextOverlap, apu8TextAddr, u32StrLen,
						(uint8_t *) pu8BGImgAddr, eColorFmt,
						u32BGWidth,
						u32BGHeight,
						(psNMVideoCtx->u32Width - u32StrLen * DIGIT_8_16_WIDTH) / 2,
						2 * DIGIT_8_16_HEIGHT);
}

static void
DateOverlap(
	H_TEXT_OVERLAP hTextOverlap,
	S_NM_VIDEOCTX *psNMVideoCtx
)
{
	//date Overlap
	char achDateTime[20];
	char achCurDateTime[17];
	time_t i32CurTime;
	struct tm *psCurTm;
	uint32_t u32Year;
	uint8_t *pu8BGImgAddr = NULL;

	if (hTextOverlap == eTEXT_OVERLAP_INVALID_HANDLE)
		return;

	if (psNMVideoCtx == NULL ||
			psNMVideoCtx->pDataVAddr == NULL )
		return;

	pu8BGImgAddr = psNMVideoCtx->pDataVAddr;

	i32CurTime = time(NULL);
	psCurTm = localtime(&i32CurTime);
	u32Year = psCurTm->tm_year + 1900;   		//since 1900

	snprintf((char *)&achCurDateTime[0x0], 16, "%04d%02d%02d%02d%02d%02d00",
			 u32Year,
			 psCurTm->tm_mon + 1,
			 psCurTm->tm_mday,
			 psCurTm->tm_hour,
			 psCurTm->tm_min,
			 psCurTm->tm_sec);
	achCurDateTime[16] = '\0';

	memcpy(&achDateTime[0], achCurDateTime, 4); //year
	achDateTime[4] = '/';
	memcpy(&achDateTime[5], achCurDateTime + 4, 2); //mouth
	achDateTime[7] = '/';
	memcpy(&achDateTime[8], achCurDateTime + 6, 2); //day

	achDateTime[10] = ' ';
	memcpy(&achDateTime[11], achCurDateTime + 8, 2); //hour
	achDateTime[13] = ':';
	memcpy(&achDateTime[14], achCurDateTime + 10, 2); //min
	achDateTime[16] = ':';
	memcpy(&achDateTime[17], achCurDateTime + 12, 2); //sec
	achDateTime[19] = ' ';
	achDateTime[20] = '\0';

	OverlapText(hTextOverlap, psNMVideoCtx, pu8BGImgAddr, achDateTime);
}

static void *jpeg_loop(void *d)
{
	struct jpeg_encoder *en = (struct jpeg_encoder *)d;
	S_NM_VIDEOCTX sVideoSrcPlannerCtx;
	S_NM_VIDEOCTX sVideoSrcPacketCtx;

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


	H_TEXT_OVERLAP hTextOverlap=eTEXT_OVERLAP_INVALID_HANDLE;

	u32DesireBitRate = en->bitrate;
	u32DesireFrameRate =  DEF_FRAME_RATE;

	//Setup video-in context
	//For planner pipe
	memset(&sVideoSrcPlannerCtx, 0x00, sizeof(S_NM_VIDEOCTX));

	sVideoSrcPlannerCtx.u32Width = en->u32VinPlannerWidth;
	sVideoSrcPlannerCtx.u32Height = en->u32VinPlannerHeight;
	sVideoSrcPlannerCtx.u32StrideW = sVideoSrcPlannerCtx.u32Width;
	sVideoSrcPlannerCtx.u32StrideH = sVideoSrcPlannerCtx.u32Height ;
	sVideoSrcPlannerCtx.eColorType = eNM_COLOR_YUV420P;

	//For packet pipe
	memset(&sVideoSrcPacketCtx, 0x00, sizeof(S_NM_VIDEOCTX));
	sVideoSrcPacketCtx.u32Width 	= en->u32VinPacketWidth;
	sVideoSrcPacketCtx.u32Height 	= en->u32VinPacketHeight;
	sVideoSrcPacketCtx.u32StrideW 	= sVideoSrcPacketCtx.u32Width;
	sVideoSrcPacketCtx.u32StrideH 	= sVideoSrcPacketCtx.u32Height;
	sVideoSrcPacketCtx.eColorType 	= eNM_COLOR_YUV422;

	//Setup JPEG encode context
	memset(&sVideoDstCtx, 0x00, sizeof(S_NM_VIDEOCTX));
	
	if ( en->u32JpegPacketPipe ) {
		//Test
		sVideoDstCtx.u32Width 	= en->u32VinPacketWidth;
		sVideoDstCtx.u32Height 	= en->u32VinPacketHeight;
		sVideoDstCtx.u32StrideW = sVideoDstCtx.u32Width;
		sVideoDstCtx.u32StrideH = sVideoDstCtx.u32Height ;
		sVideoDstCtx.eColorType = eNM_COLOR_YUV420;
		
		// Overlapping instance initialization
		if ( en->u32Overlapping )
			hTextOverlap = TextOverlap_Init(DIGIT_8_16_WIDTH,
									DIGIT_8_16_HEIGHT,
									eIMGPROC_YUV422,
									DEF_MAX_OVERLAP_TEXT,
									0xFF, 0x80, 0x80);
	} else {
		sVideoDstCtx.u32Width 	= en->u32JpegWidth;
		sVideoDstCtx.u32Height 	= en->u32JpegHeight;
		sVideoDstCtx.u32StrideW = sVideoDstCtx.u32Width;
		sVideoDstCtx.u32StrideH = sVideoDstCtx.u32Height ;
		sVideoDstCtx.eColorType = eNM_COLOR_YUV420P;	
		
		// Overlapping instance initialization
		if ( en->u32Overlapping )
			hTextOverlap = TextOverlap_Init(DIGIT_8_16_WIDTH,
									DIGIT_8_16_HEIGHT,
									eIMGPROC_YUV420P,
									DEF_MAX_OVERLAP_TEXT,
									0xFF, 0x80, 0x80);		
	}
	
	sVideoDstCtx.u32FrameRate = DEF_FRAME_RATE;
	sVideoDstCtx.u32BitRate = en->bitrate;
		
	//Open video-in device
	ERRCODE eVinErr;
	eVinErr = InitV4LDevice(&sVideoSrcPlannerCtx, &sVideoSrcPacketCtx);
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

	if (hTextOverlap == eTEXT_OVERLAP_INVALID_HANDLE)
		printf("Overlapping is disable.\n");

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
		
		// sVideoSrcPlannerCtx for encoding, sVideoSrcPacketCtx for other purpose.
		eVinErr = ReadV4LPictures(&sVideoSrcPlannerCtx, &sVideoSrcPacketCtx);

		if(eVinErr != ERR_V4L_SUCCESS){
			continue;
		}

		//Overlapping
		if ( en->u32Overlapping )
		{
			if ( en->u32JpegPacketPipe )
				DateOverlap( hTextOverlap, &sVideoSrcPacketCtx);				
			else
				DateOverlap( hTextOverlap, &sVideoSrcPlannerCtx);				
		}
		
		u64CurTime = utils_get_ms_time();

		//Encode JPEG from packet pipe
		if ( en->u32JpegPacketPipe )
			i32JpegRet = JpegEnc(&sVideoSrcPacketCtx, &sVideoDstCtx);
		else
			i32JpegRet = JpegEnc(&sVideoSrcPlannerCtx, &sVideoDstCtx);		

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

	if ( en->u32Overlapping )
		if (hTextOverlap != eTEXT_OVERLAP_INVALID_HANDLE)
			TextOverlap_UnInit(&hTextOverlap);

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

static int set_vin_planner_width(int num_tokens, struct token *tokens, void *d)
{
	struct jpeg_encoder *en = (struct jpeg_encoder *)d;

	en->u32VinPlannerWidth = tokens[1].v.num;

	return 0;
}

static int set_vin_planner_height(int num_tokens, struct token *tokens, void *d)
{
	struct jpeg_encoder *en = (struct jpeg_encoder *)d;

	en->u32VinPlannerHeight = tokens[1].v.num;

	return 0;
}

static int set_vin_packet_width(int num_tokens, struct token *tokens, void *d)
{
	struct jpeg_encoder *en = (struct jpeg_encoder *)d;

	en->u32VinPacketWidth = tokens[1].v.num;

	return 0;
}

static int set_vin_packet_height(int num_tokens, struct token *tokens, void *d)
{
	struct jpeg_encoder *en = (struct jpeg_encoder *)d;

	en->u32VinPacketHeight = tokens[1].v.num;

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

static int set_jpeg_packet(int num_tokens, struct token *tokens, void *d)
{
	struct jpeg_encoder *en = (struct jpeg_encoder *)d;

	en->u32JpegPacketPipe = tokens[1].v.num;

	return 0;
}

static int set_overlapping(int num_tokens, struct token *tokens, void *d)
{
	struct jpeg_encoder *en = (struct jpeg_encoder *)d;

	en->u32Overlapping = tokens[1].v.num;

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
	{ "vin_planner_width", set_vin_planner_width, 1, 1, { TOKEN_NUM } },
	{ "vin_planner_height", set_vin_planner_height, 1, 1, { TOKEN_NUM } },
	{ "vin_packet_width", set_vin_packet_width, 1, 1, { TOKEN_NUM } },
	{ "vin_packet_height", set_vin_packet_height, 1, 1, { TOKEN_NUM } },
	{ "jpeg_width", set_jpeg_width, 1, 1, { TOKEN_NUM } },
	{ "jpeg_height", set_jpeg_height, 1, 1, { TOKEN_NUM } },
	{ "jpeg_packet", set_jpeg_packet, 1, 1, { TOKEN_NUM } },	
	{ "overlapping", set_overlapping, 1, 1, { TOKEN_NUM } },	
	/* empty terminator -- do not remove */
	{ NULL, NULL, 0, 0, {} }
};


int jpeg_init(void)
{
	register_config_context("encoder", "jpeg", start_block, end_block,
							config_statements);
	return 0;
}


