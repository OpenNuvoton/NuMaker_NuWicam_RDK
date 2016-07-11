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
#include "../plugin_aac_encoder/plugin_aac_encoder.h"
#include "../plugin_audio_in/plugin_audio_in.h"


struct aac_encoder {
	struct stream *output;
	struct frame_exchanger *ex;
	pthread_t thread;
	int running;
	S_AAC_ENCODER_CONFIG sAACEncConfig;  //aac encode config
};

static int
ImportAACEncConfig(
	S_AAC_ENCODER_CONFIG	*psAACEncConfig
)
{
	int32_t i32TryCnt = 100;
	S_MSF_RESOURCE_DATA *psAACEncRes = NULL;

	while ((psAACEncRes == NULL) && (i32TryCnt)) {
		i32TryCnt --;
		psAACEncRes = g_sPluginIf.m_pResIf->m_pfnImportDirty(eMSF_PLUGIN_ID_AAC_ENCODER, eAAC_ENCODER_RES_CONFIG, NULL, 0);
		usleep(10000);
	}

	if (psAACEncRes == NULL)
		return -1;

	memcpy((void *)psAACEncConfig, psAACEncRes->m_pBuf, sizeof(S_AAC_ENCODER_CONFIG));
	return 0;
}



static void *aac_loop(void *d)
{
	struct aac_encoder *en = (struct aac_encoder *)d;
	struct frame *psAACFrame = NULL;
	S_MSF_RESOURCE_DATA *psAudioSrcAAC;
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

		psAudioSrcAAC = g_sPluginIf.m_pResIf->m_pfnImportWaitDirty(eMSF_PLUGIN_ID_AAC_ENCODER, eAAC_ENCODER_RES_FRAME, NULL,
						u64AudioUpdateTime);

		if (psAudioSrcAAC == NULL) {
			usleep(10000);
			continue;
		}

		psAudioDataHdr = (S_AUDIO_DATA_HDR *)psAudioSrcAAC->m_pBuf;
		u64AudioUpdateTime = psAudioSrcAAC->m_uiBufUpdateTime;

		while (1) {
			if (psAudioDataHdr->u32Delimiter != AUDIO_DATA_DELIMITER) {
				break;
			}

			u32AudioDataSize = psAudioDataHdr->u32DataLen;
			pu8AudioSrcBuf = (uint8_t *)psAudioDataHdr + sizeof(S_AUDIO_DATA_HDR);

			if (u32AudioDataSize > i32MaxFrameSize) {
				spook_log(SL_WARN, "aac: encode size large than frame size \n");
				usleep(10000);
				continue;
			}

			//There is ADTS header in bitstream, remove it!
			if ((pu8AudioSrcBuf[0] == 0xFF) && ((pu8AudioSrcBuf[1] & 0xF0) == 0xF0)) {
				if (pu8AudioSrcBuf[1] & 0x01) {
					//without CRC
					pu8AudioSrcBuf = pu8AudioSrcBuf + 7;
					u32AudioDataSize = u32AudioDataSize - 7;
				}
				else {
					//with CRC
					pu8AudioSrcBuf = pu8AudioSrcBuf + 9;
					u32AudioDataSize = u32AudioDataSize - 9;
				}
			}

			psAACFrame = new_frame();

			if (psAACFrame == NULL) {
				usleep(10000);
				continue;
			}

			memcpy(psAACFrame->d, pu8AudioSrcBuf, u32AudioDataSize);

			psAACFrame->format = FORMAT_AAC;
			psAACFrame->width = 0;
			psAACFrame->height = 0;
			psAACFrame->key = 1;
			psAACFrame->length = u32AudioDataSize;

			u64RTPTimestamp = (uint64_t)en->sAACEncConfig.m_uiSampleRate * psAudioDataHdr->u64Timestamp / 1000;

			psAACFrame->timestamp = (uint32_t)u64RTPTimestamp;

			deliver_frame(en->ex, psAACFrame);

			if (!psAudioDataHdr->u32Flag_End) {
				psAudioDataHdr = (S_AUDIO_DATA_HDR *)(pu8AudioSrcBuf + u32AudioDataSize + psAudioDataHdr->u32PadBytes);
			}
			else {
				break;
			}

			usleep(10000);
		}
	}

	return NULL;
}

static void get_framerate(struct stream *s, int *fincr, int *fbase)
{
	struct aac_encoder *en = (struct aac_encoder *)s->private;

	ImportAACEncConfig(&en->sAACEncConfig);

	*fincr = en->sAACEncConfig.m_ui16Channel;
	*fbase = en->sAACEncConfig.m_ui16Channel * en->sAACEncConfig.m_uiSampleRate;
}

static void set_running(struct stream *s, int running)
{
	struct aac_encoder *en = (struct aac_encoder *)s->private;

	en->running = running;
}

/************************ CONFIGURATION DIRECTIVES ************************/

static void *start_block(void)
{
	struct aac_encoder *en;

	en = (struct aac_encoder *)malloc(sizeof(struct aac_encoder));
	memset(en, 0x00, sizeof(struct aac_encoder));
	en->output = NULL;

	return en;
}

static int end_block(void *d)
{
	struct aac_encoder *en = (struct aac_encoder *)d;
	int32_t i32Ret;

	if (! en->output) {
		spook_log(SL_ERR, "aac: missing output stream name");
		return -1;
	}

	i32Ret = ImportAACEncConfig(&en->sAACEncConfig);

	if (i32Ret < 0)
		return i32Ret;

	//create import aac thread
	en->ex = new_exchanger(4, deliver_frame_to_stream, en->output);
	pthread_create(&en->thread, NULL, aac_loop, en);

	return 0;
}


static int set_output(int num_tokens, struct token *tokens, void *d)
{
	struct aac_encoder *en = (struct aac_encoder *)d;

	en->output = new_stream(tokens[1].v.str, FORMAT_AAC, en);

	if (! en->output) {
		spook_log(SL_ERR, "aac: unable to create stream \"%s\"",
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

int aac_init(void)
{
	register_config_context("encoder", "aac", start_block, end_block,
							config_statements);
	return 0;
}
