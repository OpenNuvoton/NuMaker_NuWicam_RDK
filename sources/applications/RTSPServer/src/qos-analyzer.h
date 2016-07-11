/*
mediastreamer2 library - modular sound and video processing and streaming

 * Copyright (C) 2011  Belledonne Communications, Grenoble, France

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __QOS_ANALYZER_H__
#define __QOS_ANALYZER_H__

#include <inttypes.h>

#include "utils.h"
#include "rtcp-parse.h"


typedef enum {
	eRATE_CTRL_ACT_DO_NOTHING,
	eRATE_CTRL_ACT_DEC_BITRATE,
	eRATE_CTRL_ACT_DEC_PACKETRATE,
	eRATE_CTRL_ACT_INC_QUALITY
} E_RATE_CTRL_ACT_TYPE;

typedef struct _S_RATE_CTRL_ACT {
	E_RATE_CTRL_ACT_TYPE eActType;
	int32_t i32Value;
} S_RATE_CTRL_ACT;


typedef struct _S_QOS_ANALYSER S_QOS_ANALYSER;
typedef struct _S_QOS_ANALYSER_DESC S_QOS_ANALYSER_DESC;

struct _S_QOS_ANALYSER_DESC {
	bool(*pfn_proc_rtcp)(S_QOS_ANALYSER *psObj, S_RTCP_MBLK *psRTCPBlk, int32_t i32ClockRate);
	void (*pfn_suggest_action)(S_QOS_ANALYSER *psObj, S_RATE_CTRL_ACT *psAction);
	bool(*pfn_has_improved)(S_QOS_ANALYSER *psObj);
	void (*pfn_uninit)(S_QOS_ANALYSER *);
};


/**
 * A QosAnalyzer is responsible to analyze RTCP feedback and suggest actions on bitrate or packet rate accordingly.
 * This is an abstract interface.
**/

struct _S_QOS_ANALYSER {
	S_QOS_ANALYSER_DESC *psDesc;
	int32_t i32RefCnt;
};


bool
QOSAnalyser_ProcRTCP(
	S_QOS_ANALYSER *psObj,
	S_RTCP_MBLK *psRTCPBlk,
	int32_t i32ClockRate
);

void
QOSAnalyser_SuggestAction(
	S_QOS_ANALYSER *psObj,
	S_RATE_CTRL_ACT *psAction
);

bool
QOSAnalyser_HasImproved(
	S_QOS_ANALYSER *psObj
);

S_QOS_ANALYSER *
QOSAnalyser_Ref(
	S_QOS_ANALYSER *psObj
);

void
QOSAnalyser_UnRef(
	S_QOS_ANALYSER *psObj
);

S_QOS_ANALYSER *
SimpleQOSAnalyserNew(
	struct rtp_endpoint *psRTPep
);

void
QOSAnalyser_SendSuggActCommand(
	struct rtp_endpoint *psRTPep,
	S_RATE_CTRL_ACT *psAction
);

#endif
