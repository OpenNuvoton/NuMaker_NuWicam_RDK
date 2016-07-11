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

#ifndef __RTCP_PARSE_H__
#define __RTCP_PARSE_H__

#include <inttypes.h>
#include <arpa/inet.h>

#include "rtp.h"

//#define __MACH_BIGENDIAN__

/* RTCP common header */

typedef enum {
	eRTCP_SR = 200,
	eRTCP_RR = 201,
	eRTCP_SDES = 202,
	eRTCP_BYE = 203,
	eRTCP_APP = 204,
	eRTCP_XR = 207
} E_RTCP_TYPE;

typedef struct {
#ifdef __MACH_BIGENDIAN__
	uint16_t u2Version: 2;
	uint16_t u1Padbit: 1;
	uint16_t u5RC: 5;
	uint16_t u8PacketType: 8;
#else
	uint16_t u5RC: 5;
	uint16_t u1Padbit: 1;
	uint16_t u2Version: 2;
	uint16_t u8PacketType: 8;
#endif
	uint16_t u16Length: 16;
} S_RTCP_COMMON_HEADER;


typedef struct S_RTCP_REPORT_BLOCK {
	uint32_t u32SSRC;
	uint32_t u32FractionCumLost;	/*fraction lost + cumulative number of packet lost*/
	uint32_t u32HighSeqNumRecv;	/*extended highest sequence number received */
	uint32_t u32InterarrivalJitter;
	uint32_t u32LastSR;			/*last SR */
	uint32_t u32DelaySncLastSR; /*delay since last sr*/
} S_RTCP_REPORT_BLOCK;


typedef struct S_RTCP_RR {
	S_RTCP_COMMON_HEADER sCommonHdr;
	uint32_t u32SSRC;
	S_RTCP_REPORT_BLOCK asReportBlock[1];
} S_RTCP_RR;

typedef struct {
	uint8_t *pu8WritePtr;
	uint8_t *pu8ReadPtr;
} S_RTCP_MBLK;


#define RTCP_COMMON_HEADER_SIZE	4

#define RTCP_COMMON_HEADER_GET_VERSION(ch)		((ch)->u2Version)
#define RTCP_COMMON_HEADER_GET_PADBIT(ch)		((ch)->u1Padbit)
#define RTCP_COMMON_HEADER_GET_RC(ch)			((ch)->u5RC)
#define RTCP_COMMON_HEADER_GET_PACKET_TYPE(ch)	((ch)->u8PacketType)
#define RTCP_COMMON_HEADER_GET_LENGTH(ch)		ntohs((ch)->u16Length)



#define REPORT_BLOCK_GET_SSRC(rb) \
	ntohl((rb)->u32SSRC)
#define REPORT_BLOCK_GET_FRACTION_LOST(rb) \
	(((uint32_t)ntohl((rb)->u32FractionCumLost))>>24)
#define REPORT_BLOCK_GET_CUM_PACKET_LOST(rb) \
	(((uint32_t)ntohl((rb)->u32FractionCumLost)) & 0xFFFFFF)
#define REPORT_BLOCK_GET_HIGH_EXT_SEQ(rb) \
	ntohl(((S_RTCP_REPORT_BLOCK*)(rb))->u32HighSeqNumRecv)
#define REPORT_BLOCK_GET_INTERARRIVAL_JITTER(rb) \
	ntohl(((S_RTCP_REPORT_BLOCK*)(rb))->u32InterarrivalJitter)
#define REPORT_BLOCK_GET_LAST_SR_TIME(rb) \
	ntohl(((S_RTCP_REPORT_BLOCK*)(rb))->u32LastSR)
#define REPORT_BLOCK_GET_LAST_SR_DELAY(rb) \
	ntohl(((S_RTCP_REPORT_BLOCK*)(rb))->u32DelaySncLastSR)



int
ProcRTCPPacket(
	struct rtp_endpoint *psRTPep,
	uint8_t *pu8Data,
	uint32_t u32Len
);

S_RTCP_REPORT_BLOCK *
RTCP_RR_GetReportBlock(
	S_RTCP_MBLK *psMBlk,
	int32_t i32Idx
);


#endif










