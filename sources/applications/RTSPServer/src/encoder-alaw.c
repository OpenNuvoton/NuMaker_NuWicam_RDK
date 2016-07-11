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
#include <inttypes.h>

#include <event.h>
#include <log.h>
#include <frame.h>
#include <stream.h>
#include <conf_parse.h>

#include "utils.h"
#include "OSS.h"

#define DEF_AUDIO_SAMPLERATE		8000
#define DEF_AUDIO_CHANNEL			1
#define DEF_AUDIO_FRAME_SIZE 		8192

struct alaw_encoder {
	struct stream *output;
	struct frame_exchanger *ex;
	pthread_t thread;
	int running;
	S_NM_AUDIOCTX sAudioCtx;
};

#define	SIGN_BIT	(0x80)		/* Sign bit for a A-law byte. */
#define	QUANT_MASK	(0xf)		/* Quantization field mask. */
#define	NSEGS		(8)		/* Number of A-law segments. */
#define	SEG_SHIFT	(4)		/* Left shift for segment number. */
#define	SEG_MASK	(0x70)		/* Segment field mask. */

static short seg_aend[8] = {0x1F, 0x3F, 0x7F, 0xFF,
			    0x1FF, 0x3FF, 0x7FF, 0xFFF};


static short
search(
	short		val,
	short		*table,
	short		size)
{
	short		i;

	for (i = 0; i < size; i++) {
		if (val <= *table++)
			return (i);
	}
	return (size);
}

/*
 * AlawEncode() - Convert a 16-bit linear PCM value to 8-bit A-law
 *
 * AlawEncode() accepts an 16-bit integer and encodes it as A-law data.
 *
 *		Linear Input Code	Compressed Code
 *	------------------------	---------------
 *	0000000wxyza			000wxyz
 *	0000001wxyza			001wxyz
 *	000001wxyzab			010wxyz
 *	00001wxyzabc			011wxyz
 *	0001wxyzabcd			100wxyz
 *	001wxyzabcde			101wxyz
 *	01wxyzabcdef			110wxyz
 *	1wxyzabcdefg			111wxyz
 *
 * For further information see John C. Bellamy's Digital Telephony, 1982,
 * John Wiley & Sons, pps 98-111 and 472-476.
 */

static unsigned char
AlawEncode(
	short		pcm_val)	/* 2's complement (16-bit range) */
{
	short		mask;
	short		seg;
	unsigned char	aval;

	pcm_val = pcm_val >> 3;

	if (pcm_val >= 0) {
		mask = 0xD5;		/* sign (7th) bit = 1 */
	} else {
		mask = 0x55;		/* sign bit = 0 */
		pcm_val = -pcm_val - 1;
	}

	/* Convert the scaled magnitude to segment number. */
	seg = search(pcm_val, seg_aend, 8);

	/* Combine the sign, segment, and quantization bits. */

	if (seg >= 8)		/* out of range, return maximum value. */
		return (unsigned char) (0x7F ^ mask);
	else {
		aval = (unsigned char) seg << SEG_SHIFT;
		if (seg < 2)
			aval |= (pcm_val >> 1) & QUANT_MASK;
		else
			aval |= (pcm_val >> seg) & QUANT_MASK;
		return (aval ^ mask);
	}
}

static void *alaw_loop(void *d)
{
	struct alaw_encoder *en = (struct alaw_encoder *)d;
	S_NM_AUDIOCTX *psAudioCtx = &en->sAudioCtx;
	struct frame *psAlawFrame = NULL;
	int i32MaxFrameSize = get_max_frame_size();
	uint32_t u32PCMFrameSize;
	ERRCODE eErr;
	uint8_t *pu8PCMBuf = NULL;

	u32PCMFrameSize = DEF_AUDIO_FRAME_SIZE;
	pu8PCMBuf = malloc(u32PCMFrameSize);

	if(pu8PCMBuf == NULL)
		return NULL;
		
	// Open ADC
	eErr = OSS_RecOpen(psAudioCtx);
	if(eErr != ERR_OSS_NONE){
		ERRPRINT("Cannot open record device\n");
		free(pu8PCMBuf);
		return NULL;
	}

	OSS_RecSetFrameSize(u32PCMFrameSize);

	psAudioCtx->pDataVAddr = pu8PCMBuf;
	psAudioCtx->u32DataLimit = u32PCMFrameSize;

	unsigned int	u32ALawDataSize;
	short			*pi16PCMDataPtr = NULL;
	unsigned char	*pbyalawDataPtr = NULL;
	unsigned int timestamp = 0;

	for (;;) {

		// Read audio data
		eErr = OSS_RecRead(psAudioCtx);
		if(eErr != ERR_OSS_NONE){
			usleep(10000);
			continue;
		}

		if (!en->running) {
			usleep(10000);
			continue;
		}

		u32ALawDataSize = psAudioCtx->u32DataSize / 2;
		pi16PCMDataPtr = (short*)(psAudioCtx->pDataVAddr);
		timestamp += u32ALawDataSize;

		if((u32ALawDataSize) > i32MaxFrameSize){
			ERRPRINT("Must enlarge RTSP frame size\n");
			continue;
		}

		psAlawFrame = new_frame();

		if (psAlawFrame == NULL) {
			usleep(10000);
			continue;
		}
		
		//encode alaw
		pbyalawDataPtr = psAlawFrame->d;
		psAlawFrame->format = FORMAT_ALAW;
		psAlawFrame->width = 0;
		psAlawFrame->height = 0;
		psAlawFrame->key = 1;
		psAlawFrame->length = u32ALawDataSize;
		psAlawFrame->timestamp = timestamp;
		
		while (u32ALawDataSize--) {
			*pbyalawDataPtr++ = AlawEncode(*pi16PCMDataPtr++);
		}
		
		//deliver frame
		deliver_frame(en->ex, psAlawFrame);
	}

	OSS_RecClose();
	return NULL;
}

static void get_framerate(struct stream *s, int *fincr, int *fbase)
{
	struct alaw_encoder *en = (struct alaw_encoder *)s->private;

	*fincr = en->sAudioCtx.u32ChannelNum;
	*fbase = en->sAudioCtx.u32ChannelNum * en->sAudioCtx.u32SampleRate;
}

static void set_running(struct stream *s, int running)
{
	struct alaw_encoder *en = (struct alaw_encoder *)s->private;

	en->running = running;
}

/************************ CONFIGURATION DIRECTIVES ************************/

static void *start_block(void)
{
	struct alaw_encoder *en;

	en = (struct alaw_encoder *)malloc(sizeof(struct alaw_encoder));
	memset(en, 0x00, sizeof(struct alaw_encoder));
	en->output = NULL;

	return en;
}

static int end_block(void *d)
{
	struct alaw_encoder *en = (struct alaw_encoder *)d;

	if (! en->output) {
		spook_log(SL_ERR, "alaw: missing output stream name");
		return -1;
	}

	en->sAudioCtx.u32ChannelNum = DEF_AUDIO_CHANNEL;
	en->sAudioCtx.u32SampleRate = DEF_AUDIO_SAMPLERATE;
	en->sAudioCtx.ePCMType = eNM_PCM_S16LE;

	//create import alaw thread
	en->ex = new_exchanger(4, deliver_frame_to_stream, en->output);
	pthread_create(&en->thread, NULL, alaw_loop, en);

	return 0;
}


static int set_output(int num_tokens, struct token *tokens, void *d)
{
	struct alaw_encoder *en = (struct alaw_encoder *)d;

	en->output = new_stream(tokens[1].v.str, FORMAT_ALAW, en);

	if (! en->output) {
		spook_log(SL_ERR, "alaw: unable to create stream \"%s\"",
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

int alaw_init(void)
{
	register_config_context("encoder", "alaw", start_block, end_block,
							config_statements);
	return 0;
}
