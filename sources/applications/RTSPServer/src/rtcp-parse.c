/*
  The oRTP library is an RTP (Realtime Transport Protocol - rfc3550) stack.
  Copyright (C) 2001  Simon MORLAT simon.morlat@linphone.org

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <sys/time.h>

#include "rtcp-parse.h"
#include "log.h"

#include "event.h"
#include "qos-analyzer.h"
#include "frame.h"

static uint64_t TimevalToNTP(const struct timeval *tv)
{
	uint64_t msw;
	uint64_t lsw;
	msw = tv->tv_sec + 0x83AA7E80; /* 0x83AA7E80 is the number of seconds from 1900 to 1970 */
	lsw = (uint32_t)((double)tv->tv_usec * (double)(((uint64_t)1) << 32) * 1.0e-6);
	return msw << 32 | lsw;
}

/* get common header; this function will also check the sanity of the packet*/
static S_RTCP_COMMON_HEADER *
RTCPGetCommonHeader(
	S_RTCP_MBLK *psMBlk
)
{
	int32_t i32Size = psMBlk->pu8WritePtr - psMBlk->pu8ReadPtr;
	S_RTCP_COMMON_HEADER *psCommonHdr;

	if (i32Size < sizeof(S_RTCP_COMMON_HEADER)) {
		spook_log(SL_VERBOSE, "Bad RTCP packet, too short.");
		return NULL;
	}

	psCommonHdr = (S_RTCP_COMMON_HEADER *)psMBlk->pu8ReadPtr;
	return psCommonHdr;
}

static int32_t
RTCPGetSize(
	S_RTCP_MBLK *psMBlk
)
{
	const S_RTCP_COMMON_HEADER *psCommonHdr = RTCPGetCommonHeader(psMBlk);

	if (psCommonHdr == NULL)
		return -1;

	return (1 + RTCP_COMMON_HEADER_GET_LENGTH(psCommonHdr)) * 4;
}

/*in case of coumpound packet, return next RTCP packet pointer*/
static uint8_t *
RTCPNextPacket(
	S_RTCP_MBLK *psMBlk
)
{
	int32_t i32NextLen = RTCPGetSize(psMBlk);

	if (i32NextLen >= 0) {
		if ((psMBlk->pu8ReadPtr + i32NextLen) < psMBlk->pu8WritePtr) {
			psMBlk->pu8ReadPtr += i32NextLen;
			return psMBlk->pu8ReadPtr;
		}
	}

	return NULL;
}

/*Receiver report accessors*/
static bool
RTCPisRR(
	S_RTCP_MBLK *psMBlk
)
{
	const S_RTCP_COMMON_HEADER *psCommonHdr = RTCPGetCommonHeader(psMBlk);

	if ((psCommonHdr != NULL) &&
			(RTCP_COMMON_HEADER_GET_PACKET_TYPE(psCommonHdr) == eRTCP_RR)) {
		if ((psMBlk->pu8WritePtr - psMBlk->pu8ReadPtr) < sizeof(S_RTCP_RR)) {
			spook_log(SL_VERBOSE, "Too short RTCP RR packet.");
			return false;
		}

		return true;
	}

	return false;
}

S_RTCP_REPORT_BLOCK *
RTCP_RR_GetReportBlock(
	S_RTCP_MBLK *psMBlk,
	int32_t i32Idx
)
{
	S_RTCP_RR *psRR = (S_RTCP_RR *) psMBlk->pu8ReadPtr;
	S_RTCP_REPORT_BLOCK *psRB = &psRR->asReportBlock[i32Idx];
	int32_t i32Size = RTCPGetSize(psMBlk);

	if (((uint8_t *)psRB) + sizeof(S_RTCP_REPORT_BLOCK) <= (psMBlk->pu8ReadPtr + i32Size)) {
		return psRB;
	}
	else {
		if (i32Idx < RTCP_COMMON_HEADER_GET_RC(&psRR->sCommonHdr)) {
			spook_log(SL_VERBOSE, "RTCP packet should include a report_block_t at pos %i but has no space for it.", i32Idx);
		}
	}

	return NULL;
}

static void ComputeRTT(
	struct rtp_endpoint *psRTPep,
	const struct timeval *psNow,
	const S_RTCP_REPORT_BLOCK *psReportBlock
)
{
	uint64_t u64CurNTP = TimevalToNTP(psNow);
	uint32_t u32ApproxNTP = (u64CurNTP >> 16) & 0xFFFFFFFF;
	uint32_t u32LastSRTime = REPORT_BLOCK_GET_LAST_SR_TIME(psReportBlock);
	uint32_t u32LastSRDelay = REPORT_BLOCK_GET_LAST_SR_DELAY(psReportBlock);

	if (u32LastSRTime != 0 && u32LastSRDelay != 0) {
		double dRTTFrac = u32ApproxNTP - u32LastSRTime - u32LastSRDelay;

		if (dRTTFrac >= 0) {
			dRTTFrac /= 65536.0;
			psRTPep->dRTT = dRTTFrac;
			/*ortp_message("rtt estimated to %f s",session->rtt);*/
		}
		else {
			spook_log(SL_VERBOSE, "Negative RTT computation, maybe due to clock adjustments.");
		}
	}
}



int
ProcRTCPPacket(
	struct rtp_endpoint *psRTPep,
	uint8_t *pu8Data,
	uint32_t u32Len
)
{
	S_RTCP_MBLK sMBlk;
	S_RTCP_COMMON_HEADER *psRTCPHeader;

	if (u32Len < RTCP_COMMON_HEADER_SIZE) {
		spook_log(SL_VERBOSE, "Receiving a too short RTCP packet");
		return 0;
	}

	psRTCPHeader = (S_RTCP_COMMON_HEADER *)pu8Data;
	sMBlk.pu8WritePtr = pu8Data + u32Len;
	sMBlk.pu8ReadPtr = pu8Data;

	if (psRTCPHeader->u2Version != 2) {
		spook_log(SL_VERBOSE, "Receiving rtcp packet with version number !=2...discarded");
		return 0;
	}

	/* compound rtcp packet can be composed by more than one rtcp message */
	do {
		struct timeval sReceptionDate;
		const S_RTCP_REPORT_BLOCK *psReportBlock;

		/* Getting the reception date from the main clock */
		gettimeofday(&sReceptionDate, NULL);

		if (RTCPisRR(&sMBlk)) {
			psReportBlock = RTCP_RR_GetReportBlock(&sMBlk, 0);

			if (psReportBlock) {
				ComputeRTT(psRTPep, &sReceptionDate, psReportBlock);

				// Do QoS analyser only for video format
				if ((psRTPep->i32Format == FORMAT_H264) || (psRTPep->i32Format == FORMAT_JPEG)) {
					if (psRTPep->pvQosAnalyser) {
						S_RATE_CTRL_ACT sAction;

						QOSAnalyser_ProcRTCP(psRTPep->pvQosAnalyser, &sMBlk, 90000);
						QOSAnalyser_SuggestAction(psRTPep->pvQosAnalyser, &sAction);
						QOSAnalyser_SendSuggActCommand(psRTPep, &sAction);
					}
				}
			}
		}
	}
	while (RTCPNextPacket(&sMBlk));

	return 0;
}



