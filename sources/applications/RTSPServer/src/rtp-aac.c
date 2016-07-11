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

struct rtp_aac {
	unsigned char *aac_data;
	int aac_len;
	int channels;
	int rate;
	unsigned int timestamp;
	uint64_t last_frame;
};

#if 0 //from ffmpeg
static audio_specific_config(AVCodecContext *avctx)
{
	PutBitContext pb;
	AACEncContext *s = avctx->priv_data;

	init_put_bits(&pb, avctx->extradata, avctx->extradata_size * 8);
	put_bits(&pb, 5, 2); //object type - AAC-LC
	put_bits(&pb, 4, s->samplerate_index); //sample rate index
	put_bits(&pb, 4, avctx->channels);
	//GASpecificConfig
	put_bits(&pb, 1, 0); //frame length - 1024 samples
	put_bits(&pb, 1, 0); //does not depend on core coder
	put_bits(&pb, 1, 0); //is not extension
	flush_put_bits(&pb);
}

#endif

#define MAX_AACSRI	12


static const int32_t s_aiAACSRI[MAX_AACSRI] = {
	96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025,  8000
};


//Get AAC sample rate index
static int
GetAACSRI(
	int32_t i32SR,
	int32_t *piSRI
)
{
	int32_t i;

	for (i = 0; i < MAX_AACSRI; i ++) {
		if (s_aiAACSRI[i] == i32SR) {
			break;
		}
	}

	if (i < MAX_AACSRI) {
		if (piSRI) {
			*piSRI = i;
		}

		return 0;
	}

	return -1;
}

static int aac_get_sdp(char *dest, int len, int payload,
					   int port, void *d)
{
	struct rtp_aac *out = (struct rtp_aac *)d;
	uint8_t au8AudioSpecConfig[2];
	int32_t i32SRI;

	GetAACSRI(out->rate, &i32SRI);

	au8AudioSpecConfig[0] = 2 << 3;					//profile id: 2 (AAC-LC)
	au8AudioSpecConfig[0] |= (i32SRI & 0x0000000F) >> 1;	//sample rate index
	au8AudioSpecConfig[1] = (i32SRI & 0x0000000F) << 7;		//sample rate index
	au8AudioSpecConfig[1] |= (out->channels & 0x0000000F) << 3;			//channels

	//Reference RFC 3640 section 3.3.6

	return snprintf(dest, len, "m=audio %d RTP/AVP %d\r\n"
					"a=rtpmap:%d MPEG4-GENERIC/%d/%d\r\n"
					"a=fmtp:%d profile-level-id=1;"
					"mode=AAC-hbr;sizelength=13;indexlength=3;"
					"indexdeltalength=3;config=%02x%02x\r\n",
					port, payload,
					payload, out->rate, out->channels,
					payload,
					au8AudioSpecConfig[0], au8AudioSpecConfig[1]);
}

static int aac_process_frame(struct frame *f, void *d)
{
	struct rtp_aac *out = (struct rtp_aac *)d;
	uint16_t u16SamplesPerBlock;
	uint64_t frames;


	u16SamplesPerBlock = 1024; //AAC default encode samples per block

	frames = ((uint64_t)f->timestamp) / u16SamplesPerBlock;

	if (frames <= out->last_frame)
		frames = out->last_frame + 1;

	out->timestamp = (unsigned int)(frames * u16SamplesPerBlock);

	out->last_frame = frames;

	out->aac_data = f->d;
	out->aac_len = f->length;

	return 1; /* always ready! */
}

static int aac_send(struct rtp_endpoint *ep, void *d)
{
	struct rtp_aac *out = (struct rtp_aac *)d;
	int i, plen;
	struct iovec v[3];
	uint8_t au8AUHeader[4];

	//Reference RFC 3640 section 3.2 and 3.3.6

	au8AUHeader[0] = 0;
	au8AUHeader[1] = 16;
	au8AUHeader[2] = out->aac_len >> 5;
	au8AUHeader[3] = (out->aac_len & 0x1F) << 3;

	v[1].iov_base = au8AUHeader;
	v[1].iov_len = 4;

	for (i = 0; i < out->aac_len; i += plen) {
		plen = out->aac_len - i;

		if (plen > ep->max_data_size) {
			plen = ep->max_data_size;
		}

		v[2].iov_base = out->aac_data + i;
		v[2].iov_len = plen;

		if (send_rtp_packet(ep, v, 3,
							out->timestamp,
							plen + i == out->aac_len) < 0)
			return -1;
	}

	return 0;

}


struct rtp_media *new_rtp_media_aac_stream(struct stream *stream)
{
	struct rtp_aac *out;

	out = (struct rtp_aac *)malloc(sizeof(struct rtp_aac));
	out->aac_len = 0;
	stream->get_framerate(stream, &out->channels, &out->rate);
	out->rate /= out->channels;
	out->timestamp = 0;
	out->last_frame = 0;

	printf("new_rtp_media_aac_stream out->rate %d, out->channels %d \n", out->rate, out->channels);

	return new_rtp_media(aac_get_sdp, NULL,
						 aac_process_frame, aac_send, out);
}
