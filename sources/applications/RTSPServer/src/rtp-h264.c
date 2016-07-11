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
#include "../plugin_h264_encoder/plugin_h264_encoder.h"

struct h264_encoder {
	struct stream *output;
	int format;
	struct frame_exchanger *ex;
	pthread_t thread;
	int running;
	S_H264_ENCODER_CONFIG sH264EncConfig;	//H264 config
	E_H264_ENCODER_RES eH264EncRes;
};	//define in encoder-h264.c

struct rtp_h264 {
	unsigned char *d;
	int len;
	int key;	// key(I) frame
	int init_done;
	int ts_incr;
	unsigned int timestamp;
	uint64_t last_frame;
	E_H264_ENCODER_RES eH264EncRes;
	uint8_t *pu8SPS;
	uint32_t u32SPSLen;
	uint8_t *pu8PPS;
	uint32_t u32PPSLen;
};


/*****************************************************************************
* b64_encode: Stolen from VLC's http.c.
* Simplified by Michael.
* Fixed edge cases and made it work from data (vs. strings) by Ryan.
*****************************************************************************/

char *base64_encode(char *out, int out_size, const uint8_t *in, int in_size)
{
	static const char b64[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	char *ret, *dst;
	unsigned i_bits = 0;
	int i_shift = 0;
	int bytes_remaining = in_size;

	if (out_size < (in_size + 2) / 3 * 4 + 1)
		return NULL;

	ret = dst = out;

	while (bytes_remaining) {
		i_bits = (i_bits << 8) + *in++;
		bytes_remaining--;
		i_shift += 8;

		do {
			*dst++ = b64[(i_bits << 6>> i_shift) & 0x3f];
			i_shift -= 6;
		}
		while (i_shift > 6 || (bytes_remaining == 0 && i_shift > 0));
	}

	while ((dst - ret) & 3)
		*dst++ = '=';

	*dst = '\0';

	return ret;
}


static void
ImportH264EncConfig(
	S_H264_ENCODER_CONFIG	*psH264EncConfig
)
{
	S_MSF_RESOURCE_DATA *psH264Res = NULL;

	while (psH264Res == NULL) {
		psH264Res = g_sPluginIf.m_pResIf->m_pfnImportDirty(eMSF_PLUGIN_ID_H264_ENCODER, eH264_ENCODER_RES_CONFIG, NULL, 0);
		usleep(10000);
	}

	memcpy((void *)psH264EncConfig, psH264Res->m_pBuf, sizeof(S_H264_ENCODER_CONFIG));
}


static int h264_process_frame(struct frame *f, void *d)
{
	struct rtp_h264 *out = (struct rtp_h264 *)d;
	uint64_t frames;

	out->d = f->d;
	out->len = f->length;
	out->key = f->key;

	frames = (utils_get_ms_time() * 90) / out->ts_incr;

	//	if(frames <= out->last_frame)
	//		frames = out->last_frame + 1;

	out->timestamp = (unsigned int)(frames * out->ts_incr);

	out->last_frame = frames;

	return out->init_done;
}



static int h264_get_sdp(char *dest, int len, int payload, int port, void *d)
{
	struct rtp_h264 *out = (struct rtp_h264 *)d;

	S_H264_ENCODER_CONFIG sH264EncConfig;
	E_H264_ENCODER_RES eH264EncRes;
	char szProfileStr[30];
	char szSPSBase64[100];
	char szPPSBase64[100];
	char szPrameterSet[200];

	eH264EncRes	= out->eH264EncRes;
	//Wait SPS ready, 3sec
	int i32TryCnt = 30;

	while (i32TryCnt) {
		ImportH264EncConfig(&sH264EncConfig);

		if (sH264EncConfig.m_asEncPipeInfo[eH264EncRes].pu8SPS != NULL) {
			break;
		}

		i32TryCnt --;
		usleep(100000);
	}

	if (i32TryCnt == 0) {
		printf("plugin_rtsp: No SPS and PPS info in H264 resource pipe%d\n", eH264EncRes);
		out->pu8SPS = NULL;
		out->u32SPSLen = 0;
		out->pu8PPS = NULL;
		out->u32PPSLen = 0;
	}
	else {
		out->pu8SPS = sH264EncConfig.m_asEncPipeInfo[eH264EncRes].pu8SPS;
		out->u32SPSLen = sH264EncConfig.m_asEncPipeInfo[eH264EncRes].u32SPSSize;
		out->pu8PPS = sH264EncConfig.m_asEncPipeInfo[eH264EncRes].pu8PPS;
		out->u32PPSLen = sH264EncConfig.m_asEncPipeInfo[eH264EncRes].u32PPSSize;
	}

	sprintf(szProfileStr, "profile-level-id=%02x%02x%02x", sH264EncConfig.m_asEncPipeInfo[eH264EncRes].u32Profile, 0,
			sH264EncConfig.m_asEncPipeInfo[eH264EncRes].u32Level);

	if ((out->u32SPSLen) && (out->u32PPSLen)) {

		base64_encode(szSPSBase64, 100, out->pu8SPS, out->u32SPSLen);
		base64_encode(szPPSBase64, 100, out->pu8PPS, out->u32PPSLen);

		sprintf(szPrameterSet, "sprop-parameter-sets=%s,%s", szSPSBase64, szPPSBase64);

		return snprintf(dest, len, "m=video %d RTP/AVP %d\r\n"
						"a=rtpmap:%d H264/90000\r\n"
						"a=fmtp:%d packetization-mode=1; %s; %s\r\n",
						port,
						payload,
						payload,
						payload,
						szProfileStr,
						szPrameterSet);
	}

	return snprintf(dest, len, "m=video %d RTP/AVP %d\r\n"
					"a=rtpmap:%d H264/90000\r\n"
					"a=fmtp:%d packetization-mode=1; %s\r\n",
					port,
					payload,
					payload,
					payload,
					szProfileStr);
}

static int h264_send(struct rtp_endpoint *ep, void *d)
{
	struct rtp_h264 *out = (struct rtp_h264 *)d;
	int i, plen;
	struct iovec v[3];
	unsigned char vhdr[2]; //FU indicator and FU header
	uint8_t u8NALHeader;


	if ((ep->packet_count == 0) && (out->key == 0)) // Skip P frame to make sure the endpoint first frame is key frame.
		return 0;

	u8NALHeader = out->d[0];

	if ((ep->packet_count == 0) && (u8NALHeader == 0x65)) { // The first frame is I frame, but without SPS and PPS
		//Try to send SPS and PPS first
		if (out->u32SPSLen) {
			v[1].iov_base = out->pu8SPS;
			v[1].iov_len = out->u32SPSLen;
			send_rtp_packet(ep, v, 2, out->timestamp, 1);
		}

		if (out->u32PPSLen) {
			v[1].iov_base = out->pu8PPS;
			v[1].iov_len = out->u32PPSLen;
			send_rtp_packet(ep, v, 2, out->timestamp, 1);
		}
	}

	if (out->len <= ep->max_data_size) {
		v[1].iov_base = out->d;
		v[1].iov_len = out->len;

		if (send_rtp_packet(ep, v, 2, out->timestamp,
							1) < 0)
			return -1;
	}
	else {
		uint8_t u8NALType;
		uint8_t u8NRI;
		uint32_t u32FULen = 2;
		unsigned char *pFrameData;
		uint32_t u32FrameLen;

		pFrameData = out->d + 1;
		u32FrameLen = out->len - 1;
		v[1].iov_base = vhdr;
		v[1].iov_len = 2;
		u8NALType = u8NALHeader & 0x1F;
		u8NRI = u8NALHeader & 0x60;
		vhdr[0] = 28; //FU Indicator; Type = 28 ---> FU-A
		vhdr[0] |= u8NRI;
		vhdr[1] = u8NALType;
		vhdr[1] |= 1 << 7; //Set start bit

		for (i = 0; i < u32FrameLen; i += plen) {
			plen = u32FrameLen - i;

			if (plen > ep->max_data_size - u32FULen)
				plen = ep->max_data_size - u32FULen;

			if ((plen + i) == u32FrameLen) {
				vhdr[1] |= 1 << 6; //Set end bit
			}

			v[2].iov_base = pFrameData + i;
			v[2].iov_len = plen;

			if (send_rtp_packet(ep, v, 3, out->timestamp,
								plen + i == u32FrameLen) < 0)
				return -1;

			vhdr[1] &= ~(1 << 7); //Clear start bit
		}
	}

	return 0;
}

struct rtp_media *new_rtp_media_h264_stream(struct stream *stream)
{
	struct rtp_h264 *out;
	int fincr, fbase;
	struct h264_encoder *encoder;

	stream->get_framerate(stream, &fincr, &fbase);
	encoder = (struct h264_encoder *)stream->private;

	out = (struct rtp_h264 *)malloc(sizeof(struct rtp_h264));
	out->init_done = 1;
	out->timestamp = 0;
	out->ts_incr = 90000 * fincr / fbase;
	out->last_frame = 0;
	out->eH264EncRes = encoder->eH264EncRes;

	return new_rtp_media(h264_get_sdp, NULL,
						 h264_process_frame, h264_send, out);
}



