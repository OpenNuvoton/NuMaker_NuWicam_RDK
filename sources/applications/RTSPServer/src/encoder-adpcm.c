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
#include "../plugin_adpcm_encoder/plugin_adpcm_encoder.h"
#include "../plugin_audio_in/plugin_audio_in.h"


struct adpcm_encoder {
	struct stream *output;
	struct frame_exchanger *ex;
	pthread_t thread;
	int running;
	S_ADPCM_ENCODER_CONFIG sAdpcmEncConfig;  //alaw encode config
};

static int
ImportAdpcmEncConfig(
	S_ADPCM_ENCODER_CONFIG	*psAdpcmEncConfig
)
{
	int32_t i32TryCnt = 100;
	S_MSF_RESOURCE_DATA *psAdpcmEncRes = NULL;

	while ((psAdpcmEncRes == NULL) && (i32TryCnt)) {
		i32TryCnt --;
		psAdpcmEncRes = g_sPluginIf.m_pResIf->m_pfnImportDirty(eMSF_PLUGIN_ID_ADPCM_ENCODER, eADPCM_ENCODER_RES_CONFIG, NULL,
						0);
		usleep(10000);
	}

	if (psAdpcmEncRes == NULL)
		return -1;

	memcpy((void *)psAdpcmEncConfig, psAdpcmEncRes->m_pBuf, sizeof(S_ADPCM_ENCODER_CONFIG));
	return 0;
}



static void *adpcm_loop(void *d)
{
	struct adpcm_encoder *en = (struct adpcm_encoder *)d;
	struct frame *psAdpcmFrame = NULL;
	S_MSF_RESOURCE_DATA *psAudioSrcAdpcm;
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

		psAudioSrcAdpcm = g_sPluginIf.m_pResIf->m_pfnImportWaitDirty(eMSF_PLUGIN_ID_ADPCM_ENCODER, eADPCM_ENCODER_RES_FRAME,
						  NULL, u64AudioUpdateTime);

		if (psAudioSrcAdpcm == NULL) {
			usleep(10000);
			continue;
		}

		psAudioDataHdr = (S_AUDIO_DATA_HDR *)psAudioSrcAdpcm->m_pBuf;
		u64AudioUpdateTime = psAudioSrcAdpcm->m_uiBufUpdateTime;


		while (1) {
			if (psAudioDataHdr->u32Delimiter != AUDIO_DATA_DELIMITER)
				break;

			u32AudioDataSize = psAudioDataHdr->u32DataLen;
			pu8AudioSrcBuf = (uint8_t *)psAudioDataHdr + sizeof(S_AUDIO_DATA_HDR);

			if (u32AudioDataSize > i32MaxFrameSize) {
				spook_log(SL_WARN, "alaw: encode size large than frame size \n");
				usleep(10000);
				continue;
			}

			psAdpcmFrame = new_frame();

			if (psAdpcmFrame == NULL) {
				usleep(10000);
				continue;
			}

			memcpy(psAdpcmFrame->d, pu8AudioSrcBuf, u32AudioDataSize);

			psAdpcmFrame->format = FORMAT_ADPCM;
			psAdpcmFrame->width = 0;
			psAdpcmFrame->height = 0;
			psAdpcmFrame->key = 1;
			psAdpcmFrame->length = u32AudioDataSize;

			u64RTPTimestamp = (uint64_t)en->sAdpcmEncConfig.m_uiSampleRate * psAudioDataHdr->u64Timestamp / 1000;
			psAdpcmFrame->timestamp = (uint32_t)u64RTPTimestamp;

			deliver_frame(en->ex, psAdpcmFrame);

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
	struct adpcm_encoder *en = (struct adpcm_encoder *)s->private;

	ImportAdpcmEncConfig(&en->sAdpcmEncConfig);

	*fincr = en->sAdpcmEncConfig.m_ui16Channel;
	*fbase = en->sAdpcmEncConfig.m_ui16Channel * en->sAdpcmEncConfig.m_uiSampleRate;
}

static void set_running(struct stream *s, int running)
{
	struct adpcm_encoder *en = (struct adpcm_encoder *)s->private;

	en->running = running;
}

/************************ CONFIGURATION DIRECTIVES ************************/

static void *start_block(void)
{
	struct adpcm_encoder *en;

	en = (struct adpcm_encoder *)malloc(sizeof(struct adpcm_encoder));
	memset(en, 0x00, sizeof(struct adpcm_encoder));
	en->output = NULL;

	return en;
}

static int end_block(void *d)
{
	struct adpcm_encoder *en = (struct adpcm_encoder *)d;
	int32_t i32Ret;

	if (! en->output) {
		spook_log(SL_ERR, "adpcm: missing output stream name");
		return -1;
	}

	i32Ret = ImportAdpcmEncConfig(&en->sAdpcmEncConfig);

	if (i32Ret < 0)
		return i32Ret;

	if (en->sAdpcmEncConfig.m_eFormat == eADPCM_IMA) {
		spook_log(SL_ERR, "adpcm: missing DVI format");
		return -1;
	}

	//create import alaw thread
	en->ex = new_exchanger(4, deliver_frame_to_stream, en->output);
	pthread_create(&en->thread, NULL, adpcm_loop, en);

	return 0;
}


static int set_output(int num_tokens, struct token *tokens, void *d)
{
	struct adpcm_encoder *en = (struct adpcm_encoder *)d;

	en->output = new_stream(tokens[1].v.str, FORMAT_ADPCM, en);

	if (! en->output) {
		spook_log(SL_ERR, "adpcm: unable to create stream \"%s\"",
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

int adpcm_init(void)
{
	register_config_context("encoder", "adpcm", start_block, end_block,
							config_statements);
	return 0;
}
