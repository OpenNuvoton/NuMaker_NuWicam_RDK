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
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>

#include <event.h>
#include <log.h>
#include <frame.h>
#include <stream.h>
#include <outputs.h>
#include <rtp.h>
#include <rtp_media.h>
#include <conf_parse.h>

#include "msf.h"
#include "utils.h"
#include "../plugin_g726_encoder/plugin_g726_encoder.h"

struct rtp_g726 {
	unsigned char *g726_data;
	int g726_len;
	int channels;
	int rate;
	unsigned int timestamp;
	uint64_t last_frame;
	uint32_t u32SamplePerBlock;
	uint32_t u32Bitrate_kbps;
	uint32_t u32BitsPerSample;
};

static int32_t
ImportG726EncConfig(
	S_G726_ENCODER_CONFIG	*psG726EncConfig
)
{
	S_MSF_RESOURCE_DATA *psG726Res = NULL;
	int32_t i32TryCnt = 30;

	while (psG726Res == NULL) {
		psG726Res = g_sPluginIf.m_pResIf->m_pfnImportDirty(eMSF_PLUGIN_ID_G726_ENCODER, eG726_ENCODER_RES_CONFIG, NULL, 0);
		i32TryCnt --;

		if (i32TryCnt <= 0)
			return -1;

		usleep(10000);
	}

	memcpy((void *)psG726EncConfig, psG726Res->m_pBuf, sizeof(S_G726_ENCODER_CONFIG));
	return 0;
}

static int g726_get_sdp(char *dest, int len, int payload,
						int port, void *d)
{
	struct rtp_g726 *out = (struct rtp_g726 *)d;
	S_G726_ENCODER_CONFIG sG726EncConfig;

	if (ImportG726EncConfig(&sG726EncConfig) < 0)
		return -1;

	out->u32SamplePerBlock = sG726EncConfig.m_ui16SmplPerFrame;

	if (sG726EncConfig.m_uiBitrate == 16) {
		out->u32Bitrate_kbps = 16;
		out->u32BitsPerSample = 2;
	}
	else if (sG726EncConfig.m_uiBitrate == 24) {
		out->u32Bitrate_kbps = 24;
		out->u32BitsPerSample = 3;
	}
	else if (sG726EncConfig.m_uiBitrate == 32) {
		out->u32Bitrate_kbps = 32;
		out->u32BitsPerSample = 4;
	}
	else if (sG726EncConfig.m_uiBitrate == 40) {
		out->u32Bitrate_kbps = 40;
		out->u32BitsPerSample = 5;
	}

	return snprintf(dest, len,
					"m=audio %d RTP/AVP %d\r\na=rtpmap:%d G726-%d/%d/%d\r\n", port, payload, payload, out->u32Bitrate_kbps, out->rate,
					out->channels);

}

static int g726_process_frame(struct frame *f, void *d)
{
	struct rtp_g726 *out = (struct rtp_g726 *)d;
	uint16_t u16SamplesPerBlock;
	uint64_t frames;

	u16SamplesPerBlock = out->u32SamplePerBlock;

	if (u16SamplesPerBlock == 0)
		return 1;

	frames = ((uint64_t)f->timestamp) / u16SamplesPerBlock;

	if (frames <= out->last_frame)
		frames = out->last_frame + 1;

	out->timestamp = (unsigned int)(frames * u16SamplesPerBlock);

	out->last_frame = frames;

	out->g726_data = f->d;
	out->g726_len = f->length;

	return 1; /* always ready! */
}

static int g726_send(struct rtp_endpoint *ep, void *d)
{
	struct rtp_g726 *out = (struct rtp_g726 *)d;
	int i, plen;
	struct iovec v[2];
	uint32_t u32MaxDataSize = ep->max_data_size;

	u32MaxDataSize = (u32MaxDataSize / out->u32Bitrate_kbps) * out->u32Bitrate_kbps;

	for (i = 0; i < out->g726_len; i += plen) {

		plen = out->g726_len - i;

		if (plen > u32MaxDataSize) {
			plen = u32MaxDataSize;
		}

		v[1].iov_base = out->g726_data + i;
		v[1].iov_len = plen;

		if (send_rtp_packet(ep, v, 2,
							out->timestamp + (i * 8 / out->u32BitsPerSample),
							0) < 0)
			return -1;
	}

	return 0;
}

struct rtp_media *new_rtp_media_g726_stream(struct stream *stream)
{
	struct rtp_g726 *out;

	out = (struct rtp_g726 *)malloc(sizeof(struct rtp_g726));
	out->g726_len = 0;
	stream->get_framerate(stream, &out->channels, &out->rate);
	out->rate /= out->channels;
	out->timestamp = 0;
	out->last_frame = 0;

	printf("new_rtp_media_g726_stream out->rate %d, out->channels %d \n", out->rate, out->channels);

	return new_rtp_media(g726_get_sdp, NULL,
						 g726_process_frame, g726_send, out);
}




