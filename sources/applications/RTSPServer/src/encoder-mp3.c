// ----------------------------------------------------------------
// Copyright (c) 2015 Nuvoton Technology Corp. All rights reserved.
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
#include "../plugin_mp3_encoder/plugin_mp3_encoder.h"
#include "../plugin_audio_in/plugin_audio_in.h"

struct mp3_encoder {
	struct stream *output;
	struct frame_exchanger *ex;
	pthread_t thread;
	int running;
	S_MP3_ENCODER_CONFIG sMP3EncConfig;  //mp3 encode config
};

static int
ImportMP3EncConfig(
	S_MP3_ENCODER_CONFIG	*psMP3EncConfig
)
{
	int32_t i32TryCnt = 100;
	S_MSF_RESOURCE_DATA *psMP3EncRes = NULL;

	while ((psMP3EncRes == NULL) && (i32TryCnt)) {
		i32TryCnt --;
		psMP3EncRes = g_sPluginIf.m_pResIf->m_pfnImportDirty(eMSF_PLUGIN_ID_MP3_ENCODER, eMP3_ENCODER_RES_FRAME, NULL, 0);
		usleep(10000);
	}

	if (psMP3EncRes == NULL)
		return -1;

	memcpy((void *)psMP3EncConfig, psMP3EncRes->m_pBuf, sizeof(S_MP3_ENCODER_CONFIG));
	return 0;
}


static void *mp3_loop(void *d)
{
	struct mp3_encoder *en = (struct mp3_encoder *)d;
	struct frame *psMP3Frame = NULL;
	S_MSF_RESOURCE_DATA *psAudioSrcMP3;
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

		psAudioSrcMP3 = g_sPluginIf.m_pResIf->m_pfnImportWaitDirty(eMSF_PLUGIN_ID_MP3_ENCODER, eMP3_ENCODER_RES_FRAME, NULL,
						u64AudioUpdateTime);

		if (psAudioSrcMP3 == NULL) {
			usleep(10000);
			continue;
		}

		psAudioDataHdr = (S_AUDIO_DATA_HDR *)psAudioSrcMP3->m_pBuf;
		u64AudioUpdateTime = psAudioSrcMP3->m_uiBufUpdateTime;

		while (1) {
			if (psAudioDataHdr->u32Delimiter != AUDIO_DATA_DELIMITER) {
				break;
			}

			u32AudioDataSize = psAudioDataHdr->u32DataLen;
			pu8AudioSrcBuf = (uint8_t *)psAudioDataHdr + sizeof(S_AUDIO_DATA_HDR);

			if (u32AudioDataSize > i32MaxFrameSize) {
				spook_log(SL_WARN, "mp3: encode size large than frame size \n");
				usleep(10000);
				continue;
			}

			if ((pu8AudioSrcBuf[0] != 0xFF) && ((pu8AudioSrcBuf[1] & 0xE0) != 0xE0)) {
				// Not a mp3 frame
				usleep(10000);
				continue;
			}

			psMP3Frame = new_frame();

			if (psMP3Frame == NULL) {
				usleep(10000);
				continue;
			}

			memcpy(psMP3Frame->d, pu8AudioSrcBuf, u32AudioDataSize);

			psMP3Frame->format = FORMAT_MPA;
			psMP3Frame->width = 0;
			psMP3Frame->height = 0;
			psMP3Frame->key = 1;
			psMP3Frame->length = u32AudioDataSize;

			u64RTPTimestamp = (uint64_t)psAudioDataHdr->u64Timestamp * 90;
			psMP3Frame->timestamp = (uint32_t)u64RTPTimestamp;

			deliver_frame(en->ex, psMP3Frame);

			if (!psAudioDataHdr->u32Flag_End) {
				psAudioDataHdr = (S_AUDIO_DATA_HDR *)(pu8AudioSrcBuf + u32AudioDataSize + psAudioDataHdr->u32PadBytes);
			}
			else {
				break;
			}

			usleep(10000);
		}
	}

	return 0;
}

static void get_framerate(struct stream *s, int *fincr, int *fbase)
{
	struct mp3_encoder *en = (struct mp3_encoder *)s->private;

	ImportMP3EncConfig(&en->sMP3EncConfig);

	*fincr = en->sMP3EncConfig.m_ui16Channel;
	*fbase = en->sMP3EncConfig.m_ui16Channel * en->sMP3EncConfig.m_uiSampleRate;
}

static void set_running(struct stream *s, int running)
{
	struct mp3_encoder *en = (struct mp3_encoder *)s->private;

	en->running = running;
}

/************************ CONFIGURATION DIRECTIVES ************************/

static void *start_block(void)
{
	struct mp3_encoder *en;

	en = (struct mp3_encoder *)malloc(sizeof(struct mp3_encoder));
	memset(en, 0x00, sizeof(struct mp3_encoder));
	en->output = NULL;

	return en;
}

static int end_block(void *d)
{
	struct mp3_encoder *en = (struct mp3_encoder *)d;
	int32_t i32Ret;

	if (! en->output) {
		spook_log(SL_ERR, "mp3: missing output stream name");
		return -1;
	}

	i32Ret = ImportMP3EncConfig(&en->sMP3EncConfig);

	if (i32Ret < 0)
		return i32Ret;

	//create import aac thread
	en->ex = new_exchanger(4, deliver_frame_to_stream, en->output);
	pthread_create(&en->thread, NULL, mp3_loop, en);

	return 0;
}

static int set_output(int num_tokens, struct token *tokens, void *d)
{
	struct mp3_encoder *en = (struct mp3_encoder *)d;

	en->output = new_stream(tokens[1].v.str, FORMAT_MPA, en);

	if (! en->output) {
		spook_log(SL_ERR, "mp3: unable to create stream \"%s\"",
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

int mp3_init(void)
{
	register_config_context("encoder", "mp3", start_block, end_block,
							config_statements);
	return 0;
}

