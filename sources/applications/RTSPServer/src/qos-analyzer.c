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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "log.h"
#include "qos-analyzer.h"
#include "rtp.h"
#include "event.h"
#include "frame.h"

/**
 * Analyses a received RTCP packet.
 * Returns TRUE is relevant information has been found in the rtcp message, FALSE otherwise.
**/

bool
QOSAnalyser_ProcRTCP(
	S_QOS_ANALYSER *psObj,
	S_RTCP_MBLK *psRTCPBlk,
	int32_t i32ClockRate
)
{
	if (psObj->psDesc->pfn_proc_rtcp) {
		return psObj->psDesc->pfn_proc_rtcp(psObj, psRTCPBlk, i32ClockRate);
	}

	spook_log(SL_VERBOSE, "Unimplemented process_rtcp() call.");
	return false;
}

void
QOSAnalyser_SuggestAction(
	S_QOS_ANALYSER *psObj,
	S_RATE_CTRL_ACT *psAction
)
{
	if (psObj->psDesc->pfn_suggest_action) {
		psObj->psDesc->pfn_suggest_action(psObj, psAction);
	}

	return;
}

bool
QOSAnalyser_HasImproved(
	S_QOS_ANALYSER *psObj
)
{
	if (psObj->psDesc->pfn_has_improved) {
		return psObj->psDesc->pfn_has_improved(psObj);
	}

	spook_log(SL_VERBOSE, "Unimplemented has_improved() call.");
	return true;
}

S_QOS_ANALYSER *
QOSAnalyser_Ref(
	S_QOS_ANALYSER *psObj
)
{
	psObj->i32RefCnt++;
	return psObj;
}

void
QOSAnalyser_UnRef(
	S_QOS_ANALYSER *psObj
)
{
	psObj->i32RefCnt--;

	if (psObj->i32RefCnt <= 0) {
		if (psObj->psDesc->pfn_uninit)
			psObj->psDesc->pfn_uninit(psObj);

		free(psObj);
	}
}

#define STATS_HISTORY 3

static const float fUnacceptableLossRate = 10;
static const int i32BigJitter = 10; /*ms */
static const float fSignificantDelay = 0.2; /*seconds*/


typedef struct {
	uint64_t u64HighSeqRecv; /*highest sequence number received*/
	float fLostPercentage; /*percentage of lost packet since last report*/
	float fIntJitter; /*interrarrival jitter */
	float fRoundTripProp; /*round trip propagation*/
} S_RTP_STATS;

const char *
RateControl_ActionTypeName(
	E_RATE_CTRL_ACT_TYPE eActionType
)
{
	switch (eActionType) {
	case eRATE_CTRL_ACT_DO_NOTHING:
		return "DoNothing";

	case eRATE_CTRL_ACT_INC_QUALITY:
		return "IncreaseQuality";

	case eRATE_CTRL_ACT_DEC_BITRATE:
		return "DecreaseBitrate";

	case eRATE_CTRL_ACT_DEC_PACKETRATE:
		return "DecreasePacketRate";
	}

	return "bad action type";
}

typedef struct _SimpleQosAnalyser {
	S_QOS_ANALYSER sParent;
	struct rtp_endpoint *psRTPep;
	uint32_t u32SSRC;
	S_RTP_STATS sStats[STATS_HISTORY];
	int32_t i32CurIndex;
	bool bRoundTripPropDoubled;
	bool bPad[3];
} SimpleQosAnalyser;

static bool
RoundTripPropDoubled(
	S_RTP_STATS *psCur,
	S_RTP_STATS *psPrev
)
{
	//ms_message("AudioBitrateController: cur=%f, prev=%f",cur->rt_prop,prev->rt_prop);
	if (psCur->fRoundTripProp >= fSignificantDelay && psPrev->fRoundTripProp > 0) {
		if (psCur->fRoundTripProp >= (psPrev->fRoundTripProp * 2.0)) {
			/*propagation doubled since last report */
			return true;
		}
	}

	return false;
}

static bool
RoundTripPropIncreased(
	SimpleQosAnalyser *psObj
)
{
	S_RTP_STATS *psCur = &psObj->sStats[psObj->i32CurIndex % STATS_HISTORY];
	S_RTP_STATS *psPrev = &psObj->sStats[(STATS_HISTORY + psObj->i32CurIndex - 1) % STATS_HISTORY];

	if (RoundTripPropDoubled(psCur, psPrev)) {
		psObj->bRoundTripPropDoubled = true;
		return true;
	}

	return false;
}

static bool
SimpleAnalyserProcRTCP(
	S_QOS_ANALYSER *psObjBase,
	S_RTCP_MBLK *psRTCPBlk,
	int32_t i32ClockRate
)
{
	SimpleQosAnalyser *psObj = (SimpleQosAnalyser *) psObjBase;
	S_RTP_STATS *psCur;
	const S_RTCP_REPORT_BLOCK *psRB = NULL;

	psRB = RTCP_RR_GetReportBlock(psRTCPBlk, 0);

	if (psRB && (REPORT_BLOCK_GET_SSRC(psRB) == psObj->u32SSRC)) {

		psObj->i32CurIndex ++;

		psCur = &psObj->sStats[psObj->i32CurIndex % STATS_HISTORY];

		psCur->u64HighSeqRecv = REPORT_BLOCK_GET_HIGH_EXT_SEQ(psRB);
		psCur->fLostPercentage = 100.0 * (float)REPORT_BLOCK_GET_FRACTION_LOST(psRB) / 256.0;
		psCur->fIntJitter = 1000.0 * (float)REPORT_BLOCK_GET_INTERARRIVAL_JITTER(psRB) / (float)i32ClockRate;
		psCur->fRoundTripProp = psObj->psRTPep->dRTT;

		spook_log(SL_INFO, "MSQosAnalyser: lost_percentage=%f, int_jitter=%f ms, rt_prop=%f sec", psCur->fLostPercentage,
				  psCur->fIntJitter, psCur->fRoundTripProp);
	}

	return psRB != NULL;
}

static void
SimpleAnalyserSuggestAction(
	S_QOS_ANALYSER *psObjBase,
	S_RATE_CTRL_ACT *psAction
)
{
	SimpleQosAnalyser *psObj = (SimpleQosAnalyser *) psObjBase;
	S_RTP_STATS *psCur = &psObj->sStats[psObj->i32CurIndex % STATS_HISTORY];


	/*big losses and big jitter */
	if (psCur->fLostPercentage >= fUnacceptableLossRate && psCur->fIntJitter >= i32BigJitter) {
		psAction->eActType = eRATE_CTRL_ACT_DEC_BITRATE;

		if (psCur->fLostPercentage > 50)
			psAction->i32Value = 50;
		else
			psAction->i32Value = (int32_t) psCur->fLostPercentage;

		spook_log(SL_INFO, "MSQosAnalyser: loss rate unacceptable and big jitter. Decreasing video bitrate %d percent.",
				  psAction->i32Value);
	}
	else if (RoundTripPropIncreased(psObj)) {
		psAction->eActType = eRATE_CTRL_ACT_DEC_BITRATE;
		psAction->i32Value = 20;
		spook_log(SL_INFO, "MSQosAnalyser: rt_prop doubled. Decreasing video bitrate %d percent.", psAction->i32Value);
	}
	else if (psCur->fLostPercentage >= fUnacceptableLossRate) {
		/*big loss rate but no jitter, and no big rtp_prop: pure lossy network*/
		psAction->eActType = eRATE_CTRL_ACT_DEC_PACKETRATE;
		spook_log(SL_INFO, "MSQosAnalyser: loss rate unacceptable.");
	}
	else {
		psAction->eActType = eRATE_CTRL_ACT_DO_NOTHING;
		spook_log(SL_INFO, "MSQosAnalyser: everything is fine.");
	}
}

static bool
SimpleAnalyserHasImproved(
	S_QOS_ANALYSER *psObjBase
)
{
	SimpleQosAnalyser *psObj = (SimpleQosAnalyser *) psObjBase;
	S_RTP_STATS *psCur = &psObj->sStats[psObj->i32CurIndex % STATS_HISTORY];
	S_RTP_STATS *psPrev = &psObj->sStats[(STATS_HISTORY + psObj->i32CurIndex - 1) % STATS_HISTORY];

	if (psPrev->fLostPercentage >= fUnacceptableLossRate) {
		if (psCur->fLostPercentage < psPrev->fLostPercentage) {
			spook_log(SL_INFO, "MSQosAnalyser: lost percentage has improved");
			return true;
		}
		else goto end;
	}

	if (psObj->bRoundTripPropDoubled && psCur->fRoundTripProp < psPrev->fRoundTripProp) {
		spook_log(SL_INFO, "MSQosAnalyser: rt prop decrased");
		psObj->bRoundTripPropDoubled = false;
		return true;
	}

end:
	spook_log(SL_INFO, "MSQosAnalyser: no improvements.");
	return false;
}

static S_QOS_ANALYSER_DESC sSimpleAnalyserDesc = {
	SimpleAnalyserProcRTCP,
	SimpleAnalyserSuggestAction,
	SimpleAnalyserHasImproved,
};


S_QOS_ANALYSER *
SimpleQOSAnalyserNew(
	struct rtp_endpoint *psRTPep
)
{
	SimpleQosAnalyser *psObj = (SimpleQosAnalyser *) malloc(sizeof(SimpleQosAnalyser));

	if (psObj == NULL)
		return NULL;

	memset(psObj, 0x00, sizeof(SimpleQosAnalyser));

	psObj->u32SSRC = psRTPep->ssrc;
	psObj->psRTPep = psRTPep;
	psObj->sParent.psDesc = &sSimpleAnalyserDesc;
	return (S_QOS_ANALYSER *)psObj;
}


void
QOSAnalyser_SendSuggActCommand(
	struct rtp_endpoint *psRTPep,
	S_RATE_CTRL_ACT *psAction
)
{
	if (psRTPep->i32Format == FORMAT_H264) {
# if 0
		S_H264_ENCODER_CMD_SUGG_RC sSuggRCCmd;
		int32_t i32Ret;

		if (psAction->eActType == eRATE_CTRL_ACT_DO_NOTHING) {
			sSuggRCCmd.eActType = eH264_SUGG_RC_ACT_DO_NOTHING;
		}
		else if (psAction->eActType == eRATE_CTRL_ACT_DEC_BITRATE) {
			sSuggRCCmd.eActType = eH264_SUGG_RC_ACT_DEC_BITRATE;
		}
		else {
			return;
		}

		sSuggRCCmd.u32Pipe = psRTPep->i32VideoEncPipe;
		sSuggRCCmd.u32Val = psAction->i32Value;

		g_sPluginIf.m_pResIf->m_pfnSendCmd(eH264_ENCODER_CMD_SUGG_RC_ACT, &sSuggRCCmd, &i32Ret);
#endif
	}
	else if (psRTPep->i32Format == FORMAT_JPEG) {
#if 0
		S_JPEG_ENCODER_CMD_SUGG_RC sSuggRCCmd;
		int32_t i32Ret;

		if (psAction->eActType == eRATE_CTRL_ACT_DO_NOTHING) {
			sSuggRCCmd.eActType = eJPEG_SUGG_RC_ACT_DO_NOTHING;
		}
		else if (psAction->eActType == eRATE_CTRL_ACT_DEC_BITRATE) {
			sSuggRCCmd.eActType = eJPEG_SUGG_RC_ACT_DEC_BITRATE;
		}
		else {
			return;
		}

		sSuggRCCmd.u32Pipe = psRTPep->i32VideoEncPipe;
		sSuggRCCmd.u32Val = psAction->i32Value;

		g_sPluginIf.m_pResIf->m_pfnSendCmd(eJPEG_ENCODER_CMD_SUGG_RC_ACT, &sSuggRCCmd, &i32Ret);
#endif
	}
}


