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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>

#include <event.h>
#include <log.h>
#include <frame.h>
#include <outputs.h>
#include <conf_parse.h>

#include "rtsp_config.h"

int spook_log_init(int min);
int config_port(int num_tokens, struct token *tokens, void *d);
void *find_context(char *szType, char *szName);
void *run_context_start_block(void *pvCtx);
int run_context_end_block(void *pvCtx, void *pvCtxData);
int run_context_statement(void *pvCtx, void *pvCtxData, struct token *t, int num_tokens);
int jpeg_init(void);
int alaw_init(void);



static int init_random(void)
{
	int fd;
	unsigned int seed;

	if ((fd = open("/dev/urandom", O_RDONLY)) < 0) {
		spook_log(SL_ERR, "unable to open /dev/urandom: %s",
				  strerror(errno));
		return -1;
	}

	if (read(fd, &seed, sizeof(seed)) < 0) {
		spook_log(SL_ERR, "unable to read from /dev/urandom: %s",
				  strerror(errno));
		return -1;
	}

	close(fd);
	srandom(seed);
	return 0;
}

void random_bytes(unsigned char *dest, int len)
{
	int i;

	for (i = 0; i < len; ++i)
		dest[i] = random() & 0xff;
}

void random_id(unsigned char *dest, int len)
{
	int i;

	for (i = 0; i < len / 2; ++i)
		sprintf((char *)dest + i * 2, "%02X",
				(unsigned int)(random() & 0xff));

	dest[len] = 0;
}

int spook_init(void)
{
	enum { DB_NONE, DB_FOREGROUND, DB_DEBUG } debug_mode = DB_NONE;

	switch (debug_mode) {
	case DB_NONE:
		spook_log_init(SL_INFO);
		break;

	case DB_FOREGROUND:
		spook_log_init(SL_VERBOSE);
		break;

	case DB_DEBUG:
		spook_log_init(SL_DEBUG);
		break;
	}

	if (init_random() < 0) return -1;

	jpeg_init();
//	h264_init();
	alaw_init();
//	adpcm_init();
//	ulaw_init();
//	aac_init();
//	g726_init();
//	mp3_init();
	live_init();


	return 0;
}





/********************* GLOBAL CONFIGURATION DIRECTIVES ********************/

int config_frameheap(int num_tokens, struct token *tokens, void *d)
{
	int size, count;

	signal(SIGPIPE, SIG_IGN);

	count = tokens[1].v.num;

	if (num_tokens == 3) size = tokens[2].v.num;
	else size = 352 * 288 * 3;

	spook_log(SL_DEBUG, "frame size is %d", size);

#if 0
	if (count < 10) {
		spook_log(SL_ERR, "frame heap of %d frames is too small, use at least 10", count);
		return -1;
	}
#endif

	init_frame_heap(size, count);

	return 0;
}

/*************************************************************************/

static int jpeg_config(
	S_RTSP_CONFIG *psConfig
)
{
	char *szCtxType = "encoder";
	char *szCtxName = "jpeg";
	struct token tokens[2];

	void *pvJpegCtx = NULL;
	void *pvCtxData = NULL;
	int i32Ret = 0;

	int i;

	pvJpegCtx = find_context(szCtxType, szCtxName);

	if (pvJpegCtx == NULL)
		return -1;

	for (i = 0; i < psConfig->m_uiNumMJPEGCams; i++) {

		pvCtxData = run_context_start_block(pvJpegCtx);

		if (pvCtxData == NULL)
			return -1;

		tokens[0].type = TOKEN_STR;
		strcpy(tokens[0].v.str, "output");
		tokens[1].type = TOKEN_STR;
		sprintf(tokens[1].v.str, "\"compressed%d\"", i);

		run_context_statement(pvJpegCtx, pvCtxData, tokens, 2);

		tokens[0].type = TOKEN_STR;
		strcpy(tokens[0].v.str, "bitrate");
		tokens[1].type = TOKEN_NUM;
		tokens[1].v.num = psConfig->m_uiMJPGCamBitRate;

		run_context_statement(pvJpegCtx, pvCtxData, tokens, 2);

		tokens[0].type = TOKEN_STR;
		strcpy(tokens[0].v.str, "pipe");
		tokens[1].type = TOKEN_NUM;
		tokens[1].v.num = i;

		run_context_statement(pvJpegCtx, pvCtxData, tokens, 2);

		tokens[0].type = TOKEN_STR;
		strcpy(tokens[0].v.str, "vin_width");
		tokens[1].type = TOKEN_NUM;
		tokens[1].v.num = psConfig->m_uiVinWidth;

		run_context_statement(pvJpegCtx, pvCtxData, tokens, 2);

		tokens[0].type = TOKEN_STR;
		strcpy(tokens[0].v.str, "vin_height");
		tokens[1].type = TOKEN_NUM;
		tokens[1].v.num = psConfig->m_uiVinHeight;

		run_context_statement(pvJpegCtx, pvCtxData, tokens, 2);

		tokens[0].type = TOKEN_STR;
		strcpy(tokens[0].v.str, "jpeg_width");
		tokens[1].type = TOKEN_NUM;
		tokens[1].v.num = psConfig->m_uiJpegWidth;

		run_context_statement(pvJpegCtx, pvCtxData, tokens, 2);

		tokens[0].type = TOKEN_STR;
		strcpy(tokens[0].v.str, "jpeg_height");
		tokens[1].type = TOKEN_NUM;
		tokens[1].v.num = psConfig->m_uiJpegHeight;

		run_context_statement(pvJpegCtx, pvCtxData, tokens, 2);

		i32Ret = run_context_end_block(pvJpegCtx, pvCtxData);

		if (i32Ret < 0)
			break;
	}

	return i32Ret;
}

#if 0
static int h264_config(
	S_RTSP_CONFIG *psConfig
)
{
	char *szCtxType = "encoder";
	char *szCtxName = "h264";
	struct token tokens[2];

	void *pvH264Ctx = NULL;
	void *pvCtxData = NULL;
	int i32Ret = 0;

	int i;

	pvH264Ctx = find_context(szCtxType, szCtxName);

	if (pvH264Ctx == NULL)
		return -1;

	for (i = 0; i < psConfig->m_uiNumH264Cams; i++) {

		pvCtxData = run_context_start_block(pvH264Ctx);

		if (pvCtxData == NULL)
			return -1;

		tokens[0].type = TOKEN_STR;
		strcpy(tokens[0].v.str, "output");
		tokens[1].type = TOKEN_STR;
		sprintf(tokens[1].v.str, "\"compressed-h264%d\"", i);

		run_context_statement(pvH264Ctx, pvCtxData, tokens, 2);

		tokens[0].type = TOKEN_STR;
		strcpy(tokens[0].v.str, "pipe");
		tokens[1].type = TOKEN_NUM;
		tokens[1].v.num = i;

		run_context_statement(pvH264Ctx, pvCtxData, tokens, 2);

		tokens[0].type = TOKEN_STR;
		strcpy(tokens[0].v.str, "lock");
		tokens[1].type = TOKEN_NUM;
		tokens[1].v.num = 0;

		run_context_statement(pvH264Ctx, pvCtxData, tokens, 2);

		i32Ret = run_context_end_block(pvH264Ctx, pvCtxData);

		if (i32Ret < 0)
			break;

	}

	return i32Ret;
}
#endif

static int alaw_config(
	S_RTSP_CONFIG *psConfig
)
{
	char *szCtxType = "encoder";
	char *szCtxName = "alaw";
	struct token tokens[2];

	void *pvAlawCtx = NULL;
	void *pvCtxData = NULL;
	int i32Ret = 0;

	pvAlawCtx = find_context(szCtxType, szCtxName);

	if (pvAlawCtx == NULL)
		return -1;

	pvCtxData = run_context_start_block(pvAlawCtx);

	if (pvCtxData == NULL)
		return -1;

	tokens[0].type = TOKEN_STR;
	strcpy(tokens[0].v.str, "output");
	tokens[1].type = TOKEN_STR;
	strcpy(tokens[1].v.str, "\"alaw\"");

	run_context_statement(pvAlawCtx, pvCtxData, tokens, 2);

	i32Ret = run_context_end_block(pvAlawCtx, pvCtxData);

	return i32Ret;
}

static int ulaw_config(
	S_RTSP_CONFIG *psConfig
)
{
	char *szCtxType = "encoder";
	char *szCtxName = "ulaw";
	struct token tokens[2];

	void *pvUlawCtx = NULL;
	void *pvCtxData = NULL;
	int i32Ret = 0;

	pvUlawCtx = find_context(szCtxType, szCtxName);

	if (pvUlawCtx == NULL)
		return -1;

	pvCtxData = run_context_start_block(pvUlawCtx);

	if (pvCtxData == NULL)
		return -1;

	tokens[0].type = TOKEN_STR;
	strcpy(tokens[0].v.str, "output");
	tokens[1].type = TOKEN_STR;
	strcpy(tokens[1].v.str, "\"ulaw\"");

	run_context_statement(pvUlawCtx, pvCtxData, tokens, 2);

	i32Ret = run_context_end_block(pvUlawCtx, pvCtxData);

	return i32Ret;
}

static int adpcm_config(
	S_RTSP_CONFIG *psConfig
)
{
	char *szCtxType = "encoder";
	char *szCtxName = "adpcm";
	struct token tokens[2];

	void *pvAdpcmCtx = NULL;
	void *pvCtxData = NULL;
	int i32Ret = 0;

	pvAdpcmCtx = find_context(szCtxType, szCtxName);

	if (pvAdpcmCtx == NULL)
		return -1;

	pvCtxData = run_context_start_block(pvAdpcmCtx);

	if (pvCtxData == NULL)
		return -1;

	tokens[0].type = TOKEN_STR;
	strcpy(tokens[0].v.str, "output");
	tokens[1].type = TOKEN_STR;
	strcpy(tokens[1].v.str, "\"adpcm\"");

	run_context_statement(pvAdpcmCtx, pvCtxData, tokens, 2);

	i32Ret = run_context_end_block(pvAdpcmCtx, pvCtxData);

	return i32Ret;
}

static int aac_config(
	S_RTSP_CONFIG *psConfig
)
{
	char *szCtxType = "encoder";
	char *szCtxName = "aac";
	struct token tokens[2];

	void *pvAACCtx = NULL;
	void *pvCtxData = NULL;
	int i32Ret = 0;

	pvAACCtx = find_context(szCtxType, szCtxName);

	if (pvAACCtx == NULL)
		return -1;

	pvCtxData = run_context_start_block(pvAACCtx);

	if (pvCtxData == NULL)
		return -1;

	tokens[0].type = TOKEN_STR;
	strcpy(tokens[0].v.str, "output");
	tokens[1].type = TOKEN_STR;
	strcpy(tokens[1].v.str, "\"aac\"");

	run_context_statement(pvAACCtx, pvCtxData, tokens, 2);

	i32Ret = run_context_end_block(pvAACCtx, pvCtxData);

	return i32Ret;
}


static int g726_config(
	S_RTSP_CONFIG *psConfig
)
{
	char *szCtxType = "encoder";
	char *szCtxName = "g726";
	struct token tokens[2];

	void *pvG726Ctx = NULL;
	void *pvCtxData = NULL;
	int i32Ret = 0;

	pvG726Ctx = find_context(szCtxType, szCtxName);

	if (pvG726Ctx == NULL)
		return -1;

	pvCtxData = run_context_start_block(pvG726Ctx);

	if (pvCtxData == NULL)
		return -1;

	tokens[0].type = TOKEN_STR;
	strcpy(tokens[0].v.str, "output");
	tokens[1].type = TOKEN_STR;
	strcpy(tokens[1].v.str, "\"g726\"");

	run_context_statement(pvG726Ctx, pvCtxData, tokens, 2);

	i32Ret = run_context_end_block(pvG726Ctx, pvCtxData);

	return i32Ret;
}

static int mp3_config(
	S_RTSP_CONFIG *psConfig
)
{
	char *szCtxType = "encoder";
	char *szCtxName = "mp3";
	struct token tokens[2];

	void *pvMP3Ctx = NULL;
	void *pvCtxData = NULL;
	int i32Ret = 0;

	pvMP3Ctx = find_context(szCtxType, szCtxName);

	if (pvMP3Ctx == NULL)
		return -1;

	pvCtxData = run_context_start_block(pvMP3Ctx);

	if (pvCtxData == NULL)
		return -1;

	tokens[0].type = TOKEN_STR;
	strcpy(tokens[0].v.str, "output");
	tokens[1].type = TOKEN_STR;
	strcpy(tokens[1].v.str, "\"mp3\"");

	run_context_statement(pvMP3Ctx, pvCtxData, tokens, 2);

	i32Ret = run_context_end_block(pvMP3Ctx, pvCtxData);

	return i32Ret;
}



static int mjpg_live_config(
	S_RTSP_CONFIG *psConfig
)
{
	char *szCtxType = "rtsp-handler";
	char *szCtxName = "live";
	struct token tokens[6];

	void *pvLiveCtx = NULL;
	void *pvCtxData = NULL;
	int i;


	pvLiveCtx = find_context(szCtxType, szCtxName);

	if (pvLiveCtx == NULL)
		return -1;

	for (i = 0; i < psConfig->m_uiNumMJPEGCams; i++) {


		pvCtxData = run_context_start_block(pvLiveCtx);

		if (pvCtxData == NULL)
			return -1;

		tokens[0].type = TOKEN_STR;
		strcpy(tokens[0].v.str, "Path");
		tokens[1].type = TOKEN_STR;

		if (i) {
			if (psConfig->m_szMJPGCamPath) {
				sprintf(tokens[1].v.str, "%s-%d", psConfig->m_szMJPGCamPath, i);
			}
			else {
				sprintf(tokens[1].v.str, "/webcam-%d", i);
			}
		}
		else {
			if (psConfig->m_szMJPGCamPath) {
				strcpy(tokens[1].v.str, psConfig->m_szMJPGCamPath);
			}
			else {
				strcpy(tokens[1].v.str, "/webcam");
			}
		}

		if (psConfig->m_szAuthRealm) {
			tokens[2].type = TOKEN_STR;
			strcpy(tokens[2].v.str, psConfig->m_szAuthRealm);
			tokens[3].type = TOKEN_STR;
			strcpy(tokens[3].v.str, psConfig->m_szAuthUserName);
			tokens[4].type = TOKEN_STR;
			strcpy(tokens[4].v.str, psConfig->m_szAuthPassword);
			run_context_statement(pvLiveCtx, pvCtxData, tokens, 5);
		}
		else {
			run_context_statement(pvLiveCtx, pvCtxData, tokens, 2);
		}

		tokens[0].type = TOKEN_STR;
		strcpy(tokens[0].v.str, "Track");
		tokens[1].type = TOKEN_STR;
		sprintf(tokens[1].v.str, "\"compressed%d\"", i);

		run_context_statement(pvLiveCtx, pvCtxData, tokens, 2);

		if (psConfig->m_eMJPGCamAudio != eRTSP_AUDIOMASK_NONE) {

			tokens[0].type = TOKEN_STR;
			strcpy(tokens[0].v.str, "Track");
			tokens[1].type = TOKEN_STR;

			if (psConfig->m_eMJPGCamAudio & eRTSP_AUDIOMASK_ALAW)
				strcpy(tokens[1].v.str, "\"alaw\"");
			else if (psConfig->m_eMJPGCamAudio & eRTSP_AUDIOMASK_ULAW)
				strcpy(tokens[1].v.str, "\"ulaw\"");
			else if (psConfig->m_eMJPGCamAudio & eRTSP_AUDIOMASK_DVIADPCM)
				strcpy(tokens[1].v.str, "\"adpcm\"");
			else if (psConfig->m_eMJPGCamAudio & eRTSP_AUDIOMASK_AAC)
				strcpy(tokens[1].v.str, "\"aac\"");
			else if (psConfig->m_eMJPGCamAudio & eRTSP_AUDIOMASK_G726)
				strcpy(tokens[1].v.str, "\"g726\"");
			else if (psConfig->m_eMJPGCamAudio & eRTSP_AUDIOMASK_MP3)
				strcpy(tokens[1].v.str, "\"mp3\"");

			run_context_statement(pvLiveCtx, pvCtxData, tokens, 2);
		}

		tokens[0].type = TOKEN_STR;
		strcpy(tokens[0].v.str, "mtu");
		tokens[1].type = TOKEN_NUM;
		tokens[1].v.num = psConfig->m_uiRTPDataTransSize;

		run_context_statement(pvLiveCtx, pvCtxData, tokens, 2);

		tokens[0].type = TOKEN_STR;
		strcpy(tokens[0].v.str, "conn");
		tokens[1].type = TOKEN_NUM;
		tokens[1].v.num = psConfig->m_auiMJPGMaxConnNum[i];

		run_context_statement(pvLiveCtx, pvCtxData, tokens, 2);

		run_context_end_block(pvLiveCtx, pvCtxData);
	}

	return 0;
}

#if 0
static int h264_live_config(
	S_RTSP_CONFIG *psConfig
)
{
	char *szCtxType = "rtsp-handler";
	char *szCtxName = "live";
	struct token tokens[6];

	void *pvLiveCtx = NULL;
	void *pvCtxData = NULL;

	int i;

	pvLiveCtx = find_context(szCtxType, szCtxName);

	if (pvLiveCtx == NULL)
		return -1;

	for (i = 0; i < psConfig->m_uiNumH264Cams; i++) {

		pvCtxData = run_context_start_block(pvLiveCtx);

		if (pvCtxData == NULL)
			return -1;

		tokens[0].type = TOKEN_STR;
		strcpy(tokens[0].v.str, "Path");
		tokens[1].type = TOKEN_STR;

		if (i) {
			if (psConfig->m_szH264CamPath) {
				sprintf(tokens[1].v.str, "%s-%d", psConfig->m_szH264CamPath, i);
			}
			else {
				sprintf(tokens[1].v.str, "/webcam-h264-%d", i);
			}
		}
		else {
			if (psConfig->m_szH264CamPath) {
				strcpy(tokens[1].v.str, psConfig->m_szH264CamPath);
			}
			else {
				strcpy(tokens[1].v.str, "/webcam-h264");
			}
		}

		if (psConfig->m_szAuthRealm) {
			tokens[2].type = TOKEN_STR;
			strcpy(tokens[2].v.str, psConfig->m_szAuthRealm);
			tokens[3].type = TOKEN_STR;
			strcpy(tokens[3].v.str, psConfig->m_szAuthUserName);
			tokens[4].type = TOKEN_STR;
			strcpy(tokens[4].v.str, psConfig->m_szAuthPassword);
			run_context_statement(pvLiveCtx, pvCtxData, tokens, 5);
		}
		else {
			run_context_statement(pvLiveCtx, pvCtxData, tokens, 2);
		}

		tokens[0].type = TOKEN_STR;
		strcpy(tokens[0].v.str, "Track");
		tokens[1].type = TOKEN_STR;
		sprintf(tokens[1].v.str, "\"compressed-h264%d\"", i);

		run_context_statement(pvLiveCtx, pvCtxData, tokens, 2);

		if (psConfig->m_eH264CamAudio != eRTSP_AUDIOMASK_NONE) {

			tokens[0].type = TOKEN_STR;
			strcpy(tokens[0].v.str, "Track");
			tokens[1].type = TOKEN_STR;

			if (psConfig->m_eH264CamAudio & eRTSP_AUDIOMASK_ALAW)
				strcpy(tokens[1].v.str, "\"alaw\"");
			else if (psConfig->m_eH264CamAudio & eRTSP_AUDIOMASK_ULAW)
				strcpy(tokens[1].v.str, "\"ulaw\"");
			else if (psConfig->m_eH264CamAudio & eRTSP_AUDIOMASK_DVIADPCM)
				strcpy(tokens[1].v.str, "\"adpcm\"");
			else if (psConfig->m_eH264CamAudio & eRTSP_AUDIOMASK_AAC)
				strcpy(tokens[1].v.str, "\"aac\"");
			else if (psConfig->m_eH264CamAudio & eRTSP_AUDIOMASK_G726)
				strcpy(tokens[1].v.str, "\"g726\"");
			else if (psConfig->m_eH264CamAudio & eRTSP_AUDIOMASK_MP3)
				strcpy(tokens[1].v.str, "\"mp3\"");

			run_context_statement(pvLiveCtx, pvCtxData, tokens, 2);
		}

		tokens[0].type = TOKEN_STR;
		strcpy(tokens[0].v.str, "mtu");
		tokens[1].type = TOKEN_NUM;
		tokens[1].v.num = psConfig->m_uiRTPDataTransSize;

		run_context_statement(pvLiveCtx, pvCtxData, tokens, 2);

		tokens[0].type = TOKEN_STR;
		strcpy(tokens[0].v.str, "conn");
		tokens[1].type = TOKEN_NUM;
		tokens[1].v.num = psConfig->m_auiH264MaxConnNum[i];

		run_context_statement(pvLiveCtx, pvCtxData, tokens, 2);

		run_context_end_block(pvLiveCtx, pvCtxData);
	}

	return 0;
}
#endif

static int audio_live_config(
	S_RTSP_CONFIG *psConfig
)
{
	char *szCtxType = "rtsp-handler";
	char *szCtxName = "live";
	struct token tokens[6];

	void *pvLiveCtx = NULL;
	void *pvCtxData = NULL;

	pvLiveCtx = find_context(szCtxType, szCtxName);

	if (pvLiveCtx == NULL)
		return -1;

	pvCtxData = run_context_start_block(pvLiveCtx);

	if (pvCtxData == NULL)
		return -1;

	tokens[0].type = TOKEN_STR;
	strcpy(tokens[0].v.str, "Path");
	tokens[1].type = TOKEN_STR;

	if (psConfig->m_szAudioStreamPath) {
		strcpy(tokens[1].v.str, psConfig->m_szAudioStreamPath);
	}
	else {
		strcpy(tokens[1].v.str, "/audio");
	}

	if (psConfig->m_szAuthRealm) {
		tokens[2].type = TOKEN_STR;
		strcpy(tokens[2].v.str, psConfig->m_szAuthRealm);
		tokens[3].type = TOKEN_STR;
		strcpy(tokens[3].v.str, psConfig->m_szAuthUserName);
		tokens[4].type = TOKEN_STR;
		strcpy(tokens[4].v.str, psConfig->m_szAuthPassword);
		run_context_statement(pvLiveCtx, pvCtxData, tokens, 5);
	}
	else {
		run_context_statement(pvLiveCtx, pvCtxData, tokens, 2);
	}

	tokens[0].type = TOKEN_STR;
	strcpy(tokens[0].v.str, "Track");
	tokens[1].type = TOKEN_STR;

	if (psConfig->m_eAudioStreamAudio & eRTSP_AUDIOMASK_ALAW)
		strcpy(tokens[1].v.str, "\"alaw\"");
	else if (psConfig->m_eAudioStreamAudio & eRTSP_AUDIOMASK_ULAW)
		strcpy(tokens[1].v.str, "\"ulaw\"");
	else if (psConfig->m_eAudioStreamAudio & eRTSP_AUDIOMASK_DVIADPCM)
		strcpy(tokens[1].v.str, "\"adpcm\"");
	else if (psConfig->m_eAudioStreamAudio & eRTSP_AUDIOMASK_AAC)
		strcpy(tokens[1].v.str, "\"aac\"");
	else if (psConfig->m_eAudioStreamAudio & eRTSP_AUDIOMASK_G726)
		strcpy(tokens[1].v.str, "\"g726\"");
	else if (psConfig->m_eAudioStreamAudio & eRTSP_AUDIOMASK_MP3)
		strcpy(tokens[1].v.str, "\"mp3\"");

	run_context_statement(pvLiveCtx, pvCtxData, tokens, 2);

	tokens[0].type = TOKEN_STR;
	strcpy(tokens[0].v.str, "mtu");
	tokens[1].type = TOKEN_NUM;
	tokens[1].v.num = psConfig->m_uiRTPDataTransSize;

	run_context_statement(pvLiveCtx, pvCtxData, tokens, 2);

	run_context_end_block(pvLiveCtx, pvCtxData);

	return 0;
}




int spook_config(
	S_RTSP_CONFIG *psConfig
)
{
	struct token tokens[3];
	int i32JpegRet;
//	int i32H264Ret;
	int i32AudioRet;

	E_RTSP_AUDIOMASK eAudioNeeded = eRTSP_AUDIOMASK_NONE;
	E_RTSP_AUDIOMASK eAudioConfiged = eRTSP_AUDIOMASK_NONE;

	//config global frame heap
	tokens[1].v.num = psConfig->m_uiFrameCnt;
	tokens[2].v.num = psConfig->m_uiFrameSize;

	if (config_frameheap(3, tokens, NULL) < 0) {
		printf("spook: Failed to configure frame heap \n");
		return -1;
	}

	//config global port
	tokens[1].v.num = 554;

	if (config_port(2, tokens, NULL) < 0) {
		printf("spook: Failed to configure RTSP port\n");
		return -1;
	}

	i32JpegRet = jpeg_config(psConfig);

	if (i32JpegRet < 0) {
		printf("spook: Failed to configure jpeg\n");
	}

#if 0
	i32H264Ret = h264_config(psConfig);

	if (i32H264Ret < 0) {
		printf("spook: Failed to configure h264\n");
	}
#endif

	eAudioNeeded = psConfig->m_eMJPGCamAudio | psConfig->m_eH264CamAudio | psConfig->m_eAudioStreamAudio;

	if (eAudioNeeded & eRTSP_AUDIOMASK_ALAW) {
		i32AudioRet = alaw_config(psConfig);

		if (i32AudioRet < 0) {
			printf("spook: Failed to configure alaw\n");
		}
		else {
			eAudioConfiged |= eRTSP_AUDIOMASK_ALAW;
		}
	}

	if (eAudioNeeded & eRTSP_AUDIOMASK_ULAW) {
		i32AudioRet = ulaw_config(psConfig);

		if (i32AudioRet < 0) {
			printf("spook: Failed to configure ulaw\n");
		}
		else {
			eAudioConfiged |= eRTSP_AUDIOMASK_ULAW;
		}
	}

	if (eAudioNeeded & eRTSP_AUDIOMASK_DVIADPCM) {
		i32AudioRet = adpcm_config(psConfig);

		if (i32AudioRet < 0) {
			printf("spook: Failed to configure dvi adpcm\n");
		}
		else {
			eAudioConfiged |= eRTSP_AUDIOMASK_DVIADPCM;
		}
	}

	if (eAudioNeeded & eRTSP_AUDIOMASK_AAC) {
		i32AudioRet = aac_config(psConfig);

		if (i32AudioRet < 0) {
			printf("spook: Failed to configure aac\n");
		}
		else {
			eAudioConfiged |= eRTSP_AUDIOMASK_AAC;
		}
	}

	if (eAudioNeeded & eRTSP_AUDIOMASK_G726) {
		i32AudioRet = g726_config(psConfig);

		if (i32AudioRet < 0) {
			printf("spook: Failed to configure g726\n");
		}
		else {
			eAudioConfiged |= eRTSP_AUDIOMASK_G726;
		}
	}

	if (eAudioNeeded & eRTSP_AUDIOMASK_MP3) {
		i32AudioRet = mp3_config(psConfig);

		if (i32AudioRet < 0) {
			printf("spook: Failed to configure mp3\n");
		}
		else {
			eAudioConfiged |= eRTSP_AUDIOMASK_MP3;
		}
	}

	//config mjpg RTSP live
	if (i32JpegRet == 0) {

		if ((psConfig->m_eMJPGCamAudio == eRTSP_AUDIOMASK_NONE) || (psConfig->m_eMJPGCamAudio & eAudioConfiged)) {
			if (mjpg_live_config(psConfig) < 0) {
				printf("spook: Failed to configure mjpg live cam\n");
			}
		}
		else {
			printf("spook: Unable to enable mjpg live cam\n");
		}
	}
	else {
		printf("spook: Unable to enable mjpg live cam\n");
	}

#if 0
	//config h264 RTSP live
	if (i32H264Ret == 0) {
		if ((psConfig->m_eH264CamAudio == eRTSP_AUDIOMASK_NONE) || (psConfig->m_eH264CamAudio & eAudioConfiged)) {
			if (h264_live_config(psConfig) < 0) {
				printf("spook: Failed to configure h264 live cam\n");
			}
		}
		else {
			printf("spook: Unable to enable h264 live cam\n");
		}
	}
	else {
		printf("spook: Unable to enable h264 live cam\n");
	}
#endif
	//config audio only RTSP live
	if (psConfig->m_eAudioStreamAudio & eAudioConfiged) {
		if (audio_live_config(psConfig) < 0) {
			printf("spook: Failed to configure audio live stream\n");
		}
	}

	return 0;
}

void spook_run(void)
{
	event_loop(0);
}

