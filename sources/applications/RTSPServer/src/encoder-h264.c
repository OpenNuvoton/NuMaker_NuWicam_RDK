// ----------------------------------------------------------------
// Copyright (c) 2013 Nuvoton Technology Corp. All rights reserved.
// ----------------------------------------------------------------

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
// MA 02110-1301, USA.


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>

#include <event.h>
#include <log.h>
#include <frame.h>
#include <stream.h>
#include <conf_parse.h>

#include "msf.h"
#include "utils.h"
#include "../plugin_h264_encoder/plugin_h264_encoder.h"

typedef enum {
	eH264_LOOP_IDLE,
	eH264_LOOP_TO_WORKING,
	eH264_LOOP_WORKING,
} E_H264_LOOP_STATE;

struct h264_encoder {
	struct stream *output;
	int format;
	struct frame_exchanger *ex;
	pthread_t thread;
	int running;
	S_H264_ENCODER_CONFIG sH264EncConfig;	//H264 config
	E_H264_ENCODER_RES eH264EncRes;
	uint8_t *pu8IFrameBuf;
	uint32_t u32IFrameBufSize;
	BOOLEAN bImportVideoLock;

	uint64_t u64StartBitRateTime;
	uint32_t u32SendDataBytes;
	void *pvH264PushHandle;

};

static int
ImportH264EncConfig(
	S_H264_ENCODER_CONFIG	*psH264EncConfig
)
{
	int32_t i32TryCnt = 100;
	S_MSF_RESOURCE_DATA *psH264Res = NULL;

	while ((psH264Res == NULL) && (i32TryCnt)) {
		i32TryCnt --;
		psH264Res = g_sPluginIf.m_pResIf->m_pfnImportDirty(eMSF_PLUGIN_ID_H264_ENCODER, eH264_ENCODER_RES_CONFIG, NULL, 0);
		usleep(10000);
	}

	if (psH264Res == NULL)
		return -1;

	memcpy((void *)psH264EncConfig, psH264Res->m_pBuf, sizeof(S_H264_ENCODER_CONFIG));
	return 0;
}


void DeliverIFrame(
	struct h264_encoder		*en,
	uint8_t 			*pu8BitStreamBuf,
	uint32_t 			u32BitStreamLen,
	S_UTIL_H264_FRAME_INFO 	*psFrameInfo
)
{
	struct frame *psH264Frame = NULL;
	int32_t i32TryCnt;
	E_UTIL_H264_NALTYPE_MASK eFrameMask;
	uint32_t u32FrameOffset;
	uint32_t u32FrameLen;

	if (u32BitStreamLen > en->u32IFrameBufSize) {
		if (en->pu8IFrameBuf) {
			free(en->pu8IFrameBuf);
			en->u32IFrameBufSize = 0;
		}

		en->pu8IFrameBuf = malloc(u32BitStreamLen + 100);

		if (en->pu8IFrameBuf) {
			en->u32IFrameBufSize = u32BitStreamLen;
		}
		else {
			en->u32IFrameBufSize = 0;
		}
	}

	if (en->pu8IFrameBuf == NULL) {
		printf("Spook DeliverIFrame: s_pu8IFrameBuf is null\n");
		return;
	}

	memcpy(en->pu8IFrameBuf, pu8BitStreamBuf, u32BitStreamLen);

	eFrameMask = psFrameInfo->eNALType;

	while (eFrameMask) {
		if (eFrameMask & eUTIL_H264_NAL_SPS) {
			u32FrameOffset = psFrameInfo->u32SPSOffset + psFrameInfo->u32SPSStartCodeLen;
			u32FrameLen = psFrameInfo->u32SPSLen - psFrameInfo->u32SPSStartCodeLen;
			eFrameMask &= (~eUTIL_H264_NAL_SPS);
		}
		else if (eFrameMask & eUTIL_H264_NAL_PPS) {
			u32FrameOffset = psFrameInfo->u32PPSOffset + psFrameInfo->u32PPSStartCodeLen;
			u32FrameLen = psFrameInfo->u32PPSLen - psFrameInfo->u32PPSStartCodeLen;
			eFrameMask &= (~eUTIL_H264_NAL_PPS);
		}
		else {
			u32FrameOffset = psFrameInfo->u32IPOffset + psFrameInfo->u32IPStartCodeLen;
			u32FrameLen = psFrameInfo->u32IPLen - psFrameInfo->u32IPStartCodeLen;
			eFrameMask = 0;
		}

		i32TryCnt = 100;

		while (i32TryCnt) {
			psH264Frame = new_frame();

			if (psH264Frame)
				break;

			i32TryCnt --;
			usleep(10000);
		}

		if (psH264Frame == NULL) {
			printf("Spook DeliverIFrame: psH264Frame is null\n");
			return;
		}

		if (psH264Frame->size < u32FrameLen) {
			// enlarge frame size
			struct frame *new_frame;
			new_frame = enlarge_frame_buffer(psH264Frame, u32FrameLen + 40);

			if (new_frame) {
				psH264Frame = new_frame;
			}
			else {
				printf("Spook DeliverIFrame: unable enlarge frame buffer size\n");
				unref_frame(psH264Frame);
				continue;
			}
		}

		memcpy(psH264Frame->d, en->pu8IFrameBuf + u32FrameOffset, u32FrameLen);

		psH264Frame->format = FORMAT_H264;
		psH264Frame->width = en->sH264EncConfig.m_asEncPipeInfo[en->eH264EncRes].m_uiWidth;
		psH264Frame->height = en->sH264EncConfig.m_asEncPipeInfo[en->eH264EncRes].m_uiHeight;
		psH264Frame->length = u32FrameLen;

		psH264Frame->key = 1;

		if (deliver_frame(en->ex, psH264Frame) != 0)
			printf("Spook DeliverIFrame: Deliver frame failed\n");
	}
}


void DeliverPFrame(
	struct h264_encoder		*en,
	uint8_t 			*pu8BitStreamBuf,
	uint32_t 			u32BitStreamLen,
	S_UTIL_H264_FRAME_INFO 	*psFrameInfo
)
{
	struct frame *psH264Frame = NULL;
	int32_t i32TryCnt;

#if 1
	i32TryCnt = 100;

	while (i32TryCnt) {
		psH264Frame = new_frame();

		if (psH264Frame)
			break;

		i32TryCnt --;
		usleep(10000);
	}

#else
	psH264Frame = new_frame();
#endif

	if (psH264Frame == NULL) {
		printf("Spook DeliverPFrame: psH264Frame is null\n");
		return;
	}

	if (psH264Frame->size < u32BitStreamLen) {
		// enlarge frame size
		struct frame *new_frame;
		new_frame = enlarge_frame_buffer(psH264Frame, u32BitStreamLen + 40);

		if (new_frame) {
			psH264Frame = new_frame;
		}
		else {
			printf("Spook DeliverPFrame: unable enlarge frame buffer size\n");
			unref_frame(psH264Frame);
			return;
		}
	}

	uint32_t u32FrameOffset = psFrameInfo->u32IPOffset + psFrameInfo->u32IPStartCodeLen;
	uint32_t u32FrameLen = psFrameInfo->u32IPLen - psFrameInfo->u32IPStartCodeLen;

	memcpy(psH264Frame->d, pu8BitStreamBuf + u32FrameOffset, u32FrameLen);

	psH264Frame->format = FORMAT_H264;
	psH264Frame->width = en->sH264EncConfig.m_asEncPipeInfo[en->eH264EncRes].m_uiWidth;
	psH264Frame->height = en->sH264EncConfig.m_asEncPipeInfo[en->eH264EncRes].m_uiHeight;
	psH264Frame->length = u32FrameLen;

	psH264Frame->key = 0;

	if (deliver_frame(en->ex, psH264Frame) != 0)
		printf("Spook DeliverPFrame: Deliver frame failed\n");
}

#if 0

static void *h264_loop(void *d)
{
	struct h264_encoder *en = (struct h264_encoder *)d;
	S_MSF_RESOURCE_DATA *psVideoIn;
	uint32_t u32FrameSize = 0;
	uint64_t u64VideoUpdateTime = 0;
	int i32MaxFrameSize = get_max_frame_size();
	E_H264_LOOP_STATE eLoopState = eH264_LOOP_IDLE;
	S_UTIL_H264_FRAME_INFO sFrameInfo;
	uint64_t u64StartBitRateTime = 0;
	uint32_t u32SendDataBytes = 0;
	uint64_t u64CurTime;
	double dCurBitRate = 0;


	ImportH264EncConfig(&en->sH264EncConfig);

	u64StartBitRateTime = utils_get_ms_time();

	for (;;) {
		if (!en->running) {
			if (eLoopState != eH264_LOOP_IDLE) {
				eLoopState = eH264_LOOP_IDLE;
			}

			usleep(10000);
			continue;
		}
		else {
			if (eLoopState == eH264_LOOP_IDLE) {
				eLoopState = eH264_LOOP_TO_WORKING;
			}
		}

		// import h264 data
		if (en->bImportVideoLock)
			psVideoIn = g_sPluginIf.m_pResIf->m_pfnImportWaitDirtyLock(eMSF_PLUGIN_ID_H264_ENCODER, en->eH264EncRes, NULL,
						u64VideoUpdateTime);
		else
			psVideoIn = g_sPluginIf.m_pResIf->m_pfnImportWaitDirty(eMSF_PLUGIN_ID_H264_ENCODER, en->eH264EncRes, NULL,
						u64VideoUpdateTime);

		if (psVideoIn == NULL) {
			usleep(10000);
			continue;
		}

		u32FrameSize = psVideoIn->m_uiBufByteSize;
		u64VideoUpdateTime = psVideoIn->m_uiBufUpdateTime;

		if (u32FrameSize > i32MaxFrameSize) {
			spook_log(SL_WARN, "H264: encode size large than frame size \n");

			if (en->bImportVideoLock)
				g_sPluginIf.m_pResIf->m_pfnImportUnlockRes(eMSF_PLUGIN_ID_H264_ENCODER, en->eH264EncRes);

			usleep(10000);
			continue;
		}

		utils_ParseH264Frame(psVideoIn->m_pBuf, u32FrameSize, &sFrameInfo);

		if (eLoopState == eH264_LOOP_TO_WORKING) {
			if (sFrameInfo.eNALType & eUTIL_H264_NAL_I) {
				eLoopState = eH264_LOOP_WORKING;
			}
			else {
				//Skip frame
				if (en->bImportVideoLock)
					g_sPluginIf.m_pResIf->m_pfnImportUnlockRes(eMSF_PLUGIN_ID_H264_ENCODER, en->eH264EncRes);

				usleep(10000);
				continue;
			}
		}

		if (sFrameInfo.eNALType & eUTIL_H264_NAL_I) {
			DeliverIFrame(en , psVideoIn->m_pBuf, u32FrameSize, &sFrameInfo);
		}
		else {
			DeliverPFrame(en , psVideoIn->m_pBuf, u32FrameSize, &sFrameInfo);
		}

		u32SendDataBytes += u32FrameSize;
		u64CurTime = utils_get_ms_time();

		if ((u64CurTime - u64StartBitRateTime) > 1000) {
			dCurBitRate = (double)(u32SendDataBytes * 8) / ((double)(u64CurTime - u64StartBitRateTime) / 1000);
			printf("RTSP send H264 bit rate %f bps\n", dCurBitRate);
			u64StartBitRateTime = utils_get_ms_time();
			u32SendDataBytes = 0;
		}
	}

	if (en->pu8IFrameBuf)
		free(en->pu8IFrameBuf);
}

#else

static void H264_PushResCB(
	const E_MSF_PLUGIN_ID	ePluginID,
	const uint32_t			uiResIndex,
	S_MSF_RESOURCE_DATA		*psVideoIn,
	void						*pvPriv
)
{
	struct h264_encoder *en = (struct h264_encoder *)pvPriv;
	uint32_t u32FrameSize = psVideoIn->m_uiBufByteSize;

#if 0
	int i32MaxFrameSize = get_max_frame_size();

	if (u32FrameSize > i32MaxFrameSize) {
		spook_log(SL_WARN, "H264: encode size large than frame size \n");
		return;
	}

#endif

	S_UTIL_H264_FRAME_INFO sFrameInfo;

	utils_ParseH264Frame(psVideoIn->m_pBuf, u32FrameSize, &sFrameInfo);

	if (sFrameInfo.eNALType & eUTIL_H264_NAL_I) {
		DeliverIFrame(en , psVideoIn->m_pBuf, u32FrameSize, &sFrameInfo);
	}
	else {
		DeliverPFrame(en , psVideoIn->m_pBuf, u32FrameSize, &sFrameInfo);
	}

	en->u32SendDataBytes += u32FrameSize;
	uint64_t u64CurTime = utils_get_ms_time();

	if ((u64CurTime - en->u64StartBitRateTime) > 1000) {
		double dCurBitRate = (double)(en->u32SendDataBytes * 8) / ((double)(u64CurTime - en->u64StartBitRateTime) / 1000);
		printf("RTSP send H264 bit rate %f bps\n", dCurBitRate);
		en->u64StartBitRateTime = utils_get_ms_time();
		en->u32SendDataBytes = 0;
	}
}

#endif

static void get_framerate(struct stream *s, int *fincr, int *fbase)
{
	struct h264_encoder *en = (struct h264_encoder *)s->private;

	ImportH264EncConfig(&en->sH264EncConfig);

	*fincr = 1;
	*fbase = en->sH264EncConfig.m_asEncPipeInfo[en->eH264EncRes].m_uiFrameRate;
}

static void set_running(struct stream *s, int running)
{
	struct h264_encoder *en = (struct h264_encoder *)s->private;

	en->running = running;

	if (running) {
		ImportH264EncConfig(&en->sH264EncConfig);

		if (en->pvH264PushHandle == NULL) {
			en->u64StartBitRateTime = utils_get_ms_time();
			en->u32SendDataBytes = 0;

			en->pvH264PushHandle = g_sPluginIf.m_pResIf->m_pfnRegPushCB(eMSF_PLUGIN_ID_H264_ENCODER, en->eH264EncRes,
								   H264_PushResCB, en);

			if (en->pvH264PushHandle == NULL) {
				printf("plugin-rtsp: unable register H264 encode callback\n");
				return;
			}

			printf("plugin-rtsp: enable h264 encode pipe %d resource \n", en->eH264EncRes);
			g_sPluginIf.m_pResIf->m_pfnSendCmd(eH264_ENCODER_CMD_ENABLE_PIPE_ENC, &en->eH264EncRes, 0);
		}
	}
	else {
		if (en->pvH264PushHandle) {

			printf("plugin-rtsp: disable h264 encode pipe %d resource \n", en->eH264EncRes);
			g_sPluginIf.m_pResIf->m_pfnSendCmd(eH264_ENCODER_CMD_DISABLE_PIPE_ENC, &en->eH264EncRes, 0);

			g_sPluginIf.m_pResIf->m_pfnUnRegPushCB(eMSF_PLUGIN_ID_H264_ENCODER, en->eH264EncRes, en->pvH264PushHandle);
			en->pvH264PushHandle = NULL;

			if (en->pu8IFrameBuf) {
				free(en->pu8IFrameBuf);
				en->pu8IFrameBuf = NULL;
				en->u32IFrameBufSize = 0;
			}
		}
	}
}

static void new_listener_coming(struct stream *s)
{
	struct h264_encoder *en = (struct h264_encoder *)s->private;
	int32_t i32Ret;
	printf("plugin-rtsp: Force h264 encoder output I frame\n");
	g_sPluginIf.m_pResIf->m_pfnSendCmd(eH264_ENCODER_CMD_FORCE_I_FRAME, &en->eH264EncRes, &i32Ret);
}

static void get_h264_enc_pipe_num(struct stream *s, int *pipe_num)
{
	struct h264_encoder *en = (struct h264_encoder *)s->private;

	printf("plugin-rtsp: get enc pipe number %i\n", en->eH264EncRes);
	*pipe_num = en->eH264EncRes;
}



/************************ CONFIGURATION DIRECTIVES ************************/

static void *start_block(void)
{
	struct h264_encoder *en;

	en = (struct h264_encoder *)malloc(sizeof(struct h264_encoder));
	memset(en, 0x00, sizeof(struct h264_encoder));
	en->output = NULL;

	return en;
}

static int end_block(void *d)
{
	struct h264_encoder *en = (struct h264_encoder *)d;
	int32_t i32Ret;

	if (! en->output) {
		spook_log(SL_ERR, "h264: missing output stream name");
		return -1;
	}

	i32Ret = ImportH264EncConfig(&en->sH264EncConfig);

	if (i32Ret < 0)
		return i32Ret;

	if (en->eH264EncRes >= en->sH264EncConfig.m_ui32WorkablePipes) {
		printf("plugin-rtsp: No H264 workable pipes \n");
		return -2;
	}

	en->ex = new_exchanger(get_max_frame_slot() * 2, deliver_frame_to_stream, en->output);
	//	en->ex = new_exchanger( 8, deliver_frame_to_stream, en->output );

	//use push callback instead of import
	//	pthread_create( &en->thread, NULL, h264_loop, en );

	return 0;
}

static int set_pipe_num(int num_tokens, struct token *tokens, void *d)
{
	struct h264_encoder *en = (struct h264_encoder *)d;

	en->eH264EncRes = tokens[1].v.num;

	return 0;
}

static int set_import_lock(int num_tokens, struct token *tokens, void *d)
{
	struct h264_encoder *en = (struct h264_encoder *)d;

	en->bImportVideoLock = tokens[1].v.num;

	return 0;
}

static int set_output(int num_tokens, struct token *tokens, void *d)
{
	struct h264_encoder *en = (struct h264_encoder *)d;

	en->output = new_stream(tokens[1].v.str, FORMAT_H264, en);

	if (! en->output) {
		spook_log(SL_ERR, "h264: unable to create stream \"%s\"",
				  tokens[1].v.str);
		return -1;
	}

	en->output->get_framerate = get_framerate;
	en->output->set_running = set_running;
	en->output->new_listener_notify = new_listener_coming;
	en->output->get_enc_pipe_num = get_h264_enc_pipe_num;
	return 0;
}

static struct statement config_statements[] = {
	/* directive name, process function, min args, max args, arg types */
	{ "output", set_output, 1, 1, { TOKEN_STR } },
	{ "pipe", set_pipe_num, 1, 1, { TOKEN_NUM } },
	{ "lock", set_import_lock, 1, 1, { TOKEN_NUM } },
	/* empty terminator -- do not remove */
	{ NULL, NULL, 0, 0, {} }
};


int h264_init(void)
{
	register_config_context("encoder", "h264", start_block, end_block,
							config_statements);
	return 0;
}


