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

struct rtp_adpcm {
	unsigned char *adpcm_data;
	int adpcm_len;
	int channels;
	int rate;
	unsigned int timestamp;
	uint64_t last_frame;
};

static int adpcm_get_sdp(char *dest, int len, int payload,
						 int port, void *d)
{
	struct rtp_adpcm *out = (struct rtp_adpcm *)d;

	if (out->rate == 8000 && out->channels == 1)
		return snprintf(dest, len, "m=audio %d RTP/AVP %d\r\na=rtpmap:%d DVI4/%d/%d\r\n", port, 5, payload, 8000, 1);
	else if (out->rate == 16000 && out->channels == 1)
		return snprintf(dest, len, "m=audio %d RTP/AVP %d\r\na=rtpmap:%d DVI4/%d/%d\r\n", port, 6, payload, 16000, 1);
	else if (out->rate == 11025 && out->channels == 1)
		return snprintf(dest, len, "m=audio %d RTP/AVP %d\r\na=rtpmap:%d DVI4/%d/%d\r\n", port, 16, payload, 11025, 1);
	else if (out->rate == 22050 && out->channels == 1)
		return snprintf(dest, len, "m=audio %d RTP/AVP %d\r\na=rtpmap:%d DVI4/%d/%d\r\n", port, 17, payload, 22050, 1);

	return -1;
}

static int adpcm_process_frame(struct frame *f, void *d)
{
	struct rtp_adpcm *out = (struct rtp_adpcm *)d;
	uint16_t u16N;
	uint16_t u16SamplesPerBlock;
	uint64_t frames;

	u16N = (f->length / 4 / out->channels) - 1;

	u16SamplesPerBlock = ((u16N * 8)) * out->channels;

	frames = (utils_get_ms_time() * (out->rate / 1000)) / u16SamplesPerBlock;

	if (frames <= out->last_frame)
		frames = out->last_frame + 1;

	out->timestamp = (unsigned int)(frames * u16SamplesPerBlock);

	out->last_frame = frames;

	out->adpcm_data = f->d;
	out->adpcm_len = f->length;

	return 1; /* always ready! */
}

static int adpcm_get_payload(int payload, void *d)
{
	struct rtp_adpcm *out = (struct rtp_adpcm *)d;

	if (out->rate == 8000 && out->channels == 1)
		return 5;
	else if (out->rate == 16000 && out->channels == 1)
		return 6;
	else if (out->rate == 11025 && out->channels == 1)
		return 16;
	else if (out->rate == 22050 && out->channels == 1)
		return 17;
	else return payload;

	return -1;
}

static int adpcm_send(struct rtp_endpoint *ep, void *d)
{
	struct rtp_adpcm *out = (struct rtp_adpcm *)d;
	int i, plen;
	struct iovec v[2];

	for (i = 0; i < out->adpcm_len; i += plen) {
		plen = out->adpcm_len - i;

		if (plen > ep->max_data_size) {
			plen = ep->max_data_size;
			//			plen -= plen % ( out->channels * out->sampsize );
		}

		v[1].iov_base = out->adpcm_data + i;
		v[1].iov_len = plen;

		if (send_rtp_packet(ep, v, 2,
							//			    out->timestamp + i / out->channels / out->sampsize,
							//			    out->timestamp + i / out->channels,
							out->timestamp,
							0) < 0)
			return -1;
	}

	return 0;

}


struct rtp_media *new_rtp_media_adpcm_stream(struct stream *stream)
{
	struct rtp_adpcm *out;

	out = (struct rtp_adpcm *)malloc(sizeof(struct rtp_adpcm));
	out->adpcm_len = 0;
	stream->get_framerate(stream, &out->channels, &out->rate);
	out->rate /= out->channels;
	out->timestamp = 0;
	out->last_frame = 0;

	printf("new_rtp_media_adpcm_stream out->rate %d, out->channels %d \n", out->rate, out->channels);


	return new_rtp_media(adpcm_get_sdp, adpcm_get_payload,
						 adpcm_process_frame, adpcm_send, out);
}
