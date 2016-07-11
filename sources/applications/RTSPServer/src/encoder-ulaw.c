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

#include <event.h>
#include <log.h>
#include <frame.h>
#include <stream.h>
#include <conf_parse.h>

#include "msf.h"
#include "utils.h"
#include "../plugin_ulaw_encoder/plugin_ulaw_encoder.h"
#include "../plugin_audio_in/plugin_audio_in.h"


struct ulaw_encoder {
	struct stream *output;
	struct frame_exchanger *ex;
	pthread_t thread;
	int running;
	S_ULAW_ENCODER_CONFIG sUlawEncConfig;  //ulaw encode config
};

static int
ImportULawEncConfig(
	S_ULAW_ENCODER_CONFIG	*psULawEncConfig
)
{
	int32_t i32TryCnt = 100;
	S_MSF_RESOURCE_DATA *psULawEncRes = NULL;

	while ((psULawEncRes == NULL) && (i32TryCnt)) {
		i32TryCnt --;
		psULawEncRes = g_sPluginIf.m_pResIf->m_pfnImportDirty(eMSF_PLUGIN_ID_ULAW_ENCODER, eULAW_ENCODER_RES_CONFIG, NULL, 0);
		usleep(10000);
	}

	if (psULawEncRes == NULL)
		return -1;

	memcpy((void *)psULawEncConfig, psULawEncRes->m_pBuf, sizeof(S_ULAW_ENCODER_CONFIG));
	return 0;
}



static void *ulaw_loop(void *d)
{
	struct ulaw_encoder *en = (struct ulaw_encoder *)d;
	struct frame *psUlawFrame = NULL;
	S_MSF_RESOURCE_DATA *psAudioSrcULAW;
	uint64_t u64AudioUpdateTime = 0;
	uint8_t *pu8AudioSrcBuf = NULL;
	uint32_t u32AudioDataSize = 0;
	int i32MaxFrameSize = get_max_frame_size();
	S_AUDIO_DATA_HDR *psAudioDataHdr;

	for (;;) {
		if (!en->running) {
			usleep(10000);
			continue;
		}

		psAudioSrcULAW = g_sPluginIf.m_pResIf->m_pfnImportWaitDirty(eMSF_PLUGIN_ID_ULAW_ENCODER, eULAW_ENCODER_RES_FRAME, NULL,
						 u64AudioUpdateTime);

		if (psAudioSrcULAW == NULL) {
			usleep(10000);
			continue;
		}

		psAudioDataHdr = (S_AUDIO_DATA_HDR *)psAudioSrcULAW->m_pBuf;
		u64AudioUpdateTime = psAudioSrcULAW->m_uiBufUpdateTime;

		while (1) {
			if (psAudioDataHdr->u32Delimiter != AUDIO_DATA_DELIMITER)
				break;

			u32AudioDataSize = psAudioDataHdr->u32DataLen;
			pu8AudioSrcBuf = (uint8_t *)psAudioDataHdr + sizeof(S_AUDIO_DATA_HDR);

			if (u32AudioDataSize > i32MaxFrameSize) {
				spook_log(SL_WARN, "ulaw: encode size large than frame size \n");
				usleep(10000);
				continue;
			}

			psUlawFrame = new_frame();

			if (psUlawFrame == NULL) {
				usleep(10000);
				continue;
			}

			memcpy(psUlawFrame->d, pu8AudioSrcBuf, u32AudioDataSize);

			psUlawFrame->format = FORMAT_ULAW;
			psUlawFrame->width = 0;
			psUlawFrame->height = 0;
			psUlawFrame->key = 1;
			psUlawFrame->length = u32AudioDataSize;

			deliver_frame(en->ex, psUlawFrame);

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
	struct ulaw_encoder *en = (struct ulaw_encoder *)s->private;

	ImportULawEncConfig(&en->sUlawEncConfig);

	*fincr = en->sUlawEncConfig.m_ui16Channel;
	*fbase = en->sUlawEncConfig.m_ui16Channel * en->sUlawEncConfig.m_uiSampleRate;
}

static void set_running(struct stream *s, int running)
{
	struct ulaw_encoder *en = (struct ulaw_encoder *)s->private;

	en->running = running;
}

/************************ CONFIGURATION DIRECTIVES ************************/

static void *start_block(void)
{
	struct ulaw_encoder *en;

	en = (struct ulaw_encoder *)malloc(sizeof(struct ulaw_encoder));
	memset(en, 0x00, sizeof(struct ulaw_encoder));
	en->output = NULL;

	return en;
}

static int end_block(void *d)
{
	struct ulaw_encoder *en = (struct ulaw_encoder *)d;
	int32_t i32Ret;

	if (! en->output) {
		spook_log(SL_ERR, "ulaw: missing output stream name");
		return -1;
	}

	i32Ret = ImportULawEncConfig(&en->sUlawEncConfig);

	if (i32Ret < 0)
		return i32Ret;

	//create import alaw thread
	en->ex = new_exchanger(4, deliver_frame_to_stream, en->output);
	pthread_create(&en->thread, NULL, ulaw_loop, en);

	return 0;
}


static int set_output(int num_tokens, struct token *tokens, void *d)
{
	struct ulaw_encoder *en = (struct ulaw_encoder *)d;

	en->output = new_stream(tokens[1].v.str, FORMAT_ULAW, en);

	if (! en->output) {
		spook_log(SL_ERR, "ulaw: unable to create stream \"%s\"",
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

int ulaw_init(void)
{
	register_config_context("encoder", "ulaw", start_block, end_block,
							config_statements);
	return 0;
}
