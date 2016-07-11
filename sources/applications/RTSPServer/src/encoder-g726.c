// ----------------------------------------------------------------
// Copyright (c) 2014 Nuvoton Technology Corp. All rights reserved.
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

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>

#include <event.h>
#include <log.h>
#include <frame.h>
#include <stream.h>
#include <conf_parse.h>

#include "msf.h"
#include "utils.h"
#include "../plugin_g726_encoder/plugin_g726_encoder.h"
#include "../plugin_audio_in/plugin_audio_in.h"

struct g726_encoder {
	struct stream *output;
	struct frame_exchanger *ex;
	pthread_t thread;
	int running;
	S_G726_ENCODER_CONFIG sG726EncConfig;  //g726 encode config
};

static int
ImportG726EncConfig(
	S_G726_ENCODER_CONFIG	*psG726EncConfig
)
{
	int32_t i32TryCnt = 100;
	S_MSF_RESOURCE_DATA *psG726EncRes = NULL;

	while ((psG726EncRes == NULL) && (i32TryCnt)) {
		i32TryCnt --;
		psG726EncRes = g_sPluginIf.m_pResIf->m_pfnImportDirty(eMSF_PLUGIN_ID_G726_ENCODER, eG726_ENCODER_RES_CONFIG, NULL, 0);
		usleep(10000);
	}

	if (psG726EncRes == NULL)
		return -1;

	memcpy((void *)psG726EncConfig, psG726EncRes->m_pBuf, sizeof(S_G726_ENCODER_CONFIG));
	return 0;
}

static void *g726_loop(void *d)
{
	struct g726_encoder *en = (struct g726_encoder *)d;
	struct frame *psG726Frame = NULL;
	S_MSF_RESOURCE_DATA *psAudioSrcG726;
	uint64_t u64AudioUpdateTime = 0;
	uint8_t *pu8AudioSrcBuf = NULL;
	uint32_t u32AudioDataSize = 0;
	int i32MaxFrameSize = get_max_frame_size();
	S_AUDIO_DATA_HDR *psAudioDataHdr;
	uint64_t u64RTPTimestamp;

	for (;;) {
		if (!en->running) {
			usleep(10000);
			continue;
		}

		psAudioSrcG726 = g_sPluginIf.m_pResIf->m_pfnImportWaitDirty(eMSF_PLUGIN_ID_G726_ENCODER, eG726_ENCODER_RES_FRAME, NULL,
						 u64AudioUpdateTime);

		if (psAudioSrcG726 == NULL) {
			usleep(10000);
			continue;
		}

		psAudioDataHdr = (S_AUDIO_DATA_HDR *)psAudioSrcG726->m_pBuf;
		u64AudioUpdateTime = psAudioSrcG726->m_uiBufUpdateTime;

		while (1) {
			if (psAudioDataHdr->u32Delimiter != AUDIO_DATA_DELIMITER) {
				break;
			}

			u32AudioDataSize = psAudioDataHdr->u32DataLen;
			pu8AudioSrcBuf = (uint8_t *)psAudioDataHdr + sizeof(S_AUDIO_DATA_HDR);

			if (u32AudioDataSize > i32MaxFrameSize) {
				spook_log(SL_WARN, "g726: encode size large than frame size \n");
				usleep(10000);
				continue;
			}

			psG726Frame = new_frame();

			if (psG726Frame == NULL) {
				usleep(10000);
				continue;
			}

			memcpy(psG726Frame->d, pu8AudioSrcBuf, u32AudioDataSize);

			psG726Frame->format = FORMAT_G726;
			psG726Frame->width = 0;
			psG726Frame->height = 0;
			psG726Frame->key = 1;
			psG726Frame->length = u32AudioDataSize;

			u64RTPTimestamp = (uint64_t)en->sG726EncConfig.m_uiSampleRate * psAudioDataHdr->u64Timestamp / 1000;

			psG726Frame->timestamp = (uint32_t)u64RTPTimestamp;

			deliver_frame(en->ex, psG726Frame);

			if (!psAudioDataHdr->u32Flag_End) {
				psAudioDataHdr = (S_AUDIO_DATA_HDR *)(pu8AudioSrcBuf + u32AudioDataSize + psAudioDataHdr->u32PadBytes);
			}
			else {
				break;
			}

			//			usleep(10000);
		}

	}

	return NULL;
}



static void get_framerate(struct stream *s, int *fincr, int *fbase)
{
	struct g726_encoder *en = (struct g726_encoder *)s->private;

	ImportG726EncConfig(&en->sG726EncConfig);

	*fincr = en->sG726EncConfig.m_ui16Channel;
	*fbase = en->sG726EncConfig.m_ui16Channel * en->sG726EncConfig.m_uiSampleRate;
}

static void set_running(struct stream *s, int running)
{
	struct g726_encoder *en = (struct g726_encoder *)s->private;

	en->running = running;
}



/************************ CONFIGURATION DIRECTIVES ************************/

static void *start_block(void)
{
	struct g726_encoder *en;

	en = (struct g726_encoder *)malloc(sizeof(struct g726_encoder));
	memset(en, 0x00, sizeof(struct g726_encoder));
	en->output = NULL;

	return en;
}

static int end_block(void *d)
{
	struct g726_encoder *en = (struct g726_encoder *)d;
	int32_t i32Ret;

	if (! en->output) {
		spook_log(SL_ERR, "g726: missing output stream name");
		return -1;
	}

	i32Ret = ImportG726EncConfig(&en->sG726EncConfig);

	if (i32Ret < 0)
		return i32Ret;

	//create import g726 thread
	en->ex = new_exchanger(4, deliver_frame_to_stream, en->output);
	pthread_create(&en->thread, NULL, g726_loop, en);

	return 0;
}


static int set_output(int num_tokens, struct token *tokens, void *d)
{
	struct g726_encoder *en = (struct g726_encoder *)d;

	en->output = new_stream(tokens[1].v.str, FORMAT_G726, en);

	if (! en->output) {
		spook_log(SL_ERR, "g726: unable to create stream \"%s\"",
				  tokens[1].v.str);
		return -1;
	}

	en->output->get_framerate = get_framerate;
	en->output->set_running = set_running;
	return 0;
}

static struct statement config_statements[] = {
	/* directive name, process function, min args, max args, arg types */
	{ "output", set_output, 1, 1, { TOKEN_STR } },

	/* empty terminator -- do not remove */
	{ NULL, NULL, 0, 0, {} }
};

int g726_init(void)
{
	register_config_context("encoder", "g726", start_block, end_block,
							config_statements);
	return 0;
}




