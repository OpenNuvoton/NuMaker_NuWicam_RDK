//------------------------------------------------------------------
//
// Copyright (c) Nuvoton Technology Corp. All rights reserved.
//
//------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#include "utils.h"
#include "OSS.h"

//typedef struct {
//	void *pvReserved;
//} S_CTXOSS_RES;

uint32_t	g_u32NMPlayer_Volume;

//Play
static int32_t s_i32DSPFD = -1;
static int32_t s_i32MixerFD = -1;
static int32_t s_i32DSPBlockSize = 0;
static pthread_mutex_t s_sOSSFunMutex = PTHREAD_MUTEX_INITIALIZER;
static int32_t s_i32PlayRefCnt = 0;

//Record
#define REC_BLOCK_SIZE 8192
#define REC_TEMP_BUF_BLOCK 2

#define DEF_OSS_MIC	 	"/dev/dsp"
#define DEF_OSS_SPK	 	"/dev/dsp"
#define DEF_OSS_MIXER	"/dev/mixer"

static int32_t s_i32RecRefCnt = 0;

typedef struct {
	int32_t i32RecFD;
	uint64_t u64FirstTimeStamp;
	uint64_t u64NextTimeStamp;
	uint32_t u32DrvFrameSize;
	int32_t i32PrevFrames;
	int32_t i32OutPrevFrames;
	uint8_t	*pu8RecvAudBuf;
	uint8_t	*pu8PreAudTempBuf;
	uint32_t u32PreAudTempBufSize;
	uint32_t u32PreAudLenInBuf;
	uint8_t	*pu8PostAudTempBuf;
	uint32_t u32PostAudTempBufSize;
	uint32_t u32PostAudLenInBuf;
	uint32_t u32ZeroAudLenInBuf;
	uint32_t u32OutFrameSize;
} S_OSS_REC_DATA;

static S_OSS_REC_DATA s_sRecData = {
	.i32RecFD = -1,
	.u64FirstTimeStamp = 0,
	.u32DrvFrameSize = REC_BLOCK_SIZE,
	.u32ZeroAudLenInBuf = 0,
};

static E_KERNEL_VER s_eKernelVer = eKERNEL_UNKNOW;
static FILE *s_psTimeFile = NULL; //TODO: close time file (/proc/uptime)???


static E_KERNEL_VER
Util_GetKernelVer(void)
{
	FILE *psKernelVerFile = NULL;
	char tmp[100]={0x0};

	if(s_eKernelVer < eKERNEL_UNKNOW)
		return s_eKernelVer;

	s_eKernelVer = eKERNEL_UNKNOW;
	if(psKernelVerFile == NULL){
		psKernelVerFile = fopen("/proc/version", "r");
		if(psKernelVerFile == NULL){			
			perror("fopen(/proc/cpuinfo)");
			return s_eKernelVer;
		}
 	}

	fseek(psKernelVerFile, 0, SEEK_SET);

	while(fgets(tmp, sizeof(tmp), psKernelVerFile)){
		if(strstr(tmp, "2.6.17.14")){
			s_eKernelVer = eKERNEL_2_6_17_14;
			break;
		}
		if(strstr(tmp, "2.6.35.4")){
			s_eKernelVer = eKERNEL_2_6_35_4;
			break;
		}
	}

	fclose(psKernelVerFile);
	if(s_eKernelVer == eKERNEL_UNKNOW)
		perror("Get kernel version is unknow\n");
	return s_eKernelVer;
}


uint64_t
Util_GetTime(void)
{
	double retval=0;
	char tmp[64]={0x0};
	uint64_t u64MillSec = 0;
	if(s_psTimeFile == NULL){
		s_psTimeFile = fopen("/proc/uptime", "r");
		if(s_psTimeFile == NULL){			
			perror("fopen(/proc/uptime)");
			return 0;
		}
 	}
	
	while(u64MillSec == 0){
		fseek(s_psTimeFile, 0, SEEK_SET);
		fflush(s_psTimeFile);
		fgets(tmp, sizeof(tmp), s_psTimeFile);
		retval=atof(tmp);
		//fscanf
		u64MillSec = (uint64_t)(retval*1000);
		if(u64MillSec == 0){
			ERRPRINT("Get time, but return value is %f \n", retval);
		}
	}

	return u64MillSec;
}


static void
PutAudioToBuf(
	uint8_t *pu8AudData,
	uint32_t u32DataLen
) {
	if (s_sRecData.u32ZeroAudLenInBuf) { //put it in post buffer
		memcpy((s_sRecData.pu8PostAudTempBuf + s_sRecData.u32PostAudLenInBuf), pu8AudData, u32DataLen);
		s_sRecData.u32PostAudLenInBuf += u32DataLen;
		return;
	}

	memcpy((s_sRecData.pu8PreAudTempBuf + s_sRecData.u32PreAudLenInBuf), pu8AudData, u32DataLen);
	s_sRecData.u32PreAudLenInBuf += u32DataLen;
}



static bool
GetAudioFromBuf(
//	S_AVDEV_AUD_PARM *psAudParm,
	uint8_t *pu8AudData,
	uint32_t u32DataLen,
	uint64_t *pu64TimeStamp,
	uint32_t u32Channels,
	uint32_t u32SampleRate
) {
	uint8_t *pu8TempBuf = NULL;
	uint32_t *pu32TempBufLen = 0;
	uint32_t u32TempBufLen = 0;
	uint32_t u32GetedLen = 0;
	uint32_t u32CopyLen;
	uint32_t u32TotalBytesInBuf;
	int32_t i32bps = 0;
	uint64_t u64CurTime;
	int32_t i32BDelay;
	uint32_t u32Frames;
	u32TotalBytesInBuf = s_sRecData.u32PreAudLenInBuf + s_sRecData.u32ZeroAudLenInBuf + s_sRecData.u32PostAudLenInBuf;

	if ((u32TotalBytesInBuf) < u32DataLen) {
		return false;
	}

	double dBDelayTime;
	u64CurTime = Util_GetTime();
	i32bps = u32SampleRate * 16 * u32Channels;
	i32BDelay = u32TotalBytesInBuf / (16 / 8);
	dBDelayTime = (double)(i32BDelay * 1000) / (double)(u32SampleRate * u32Channels);
	u64CurTime -= (uint64_t)dBDelayTime;

	while (u32GetedLen < u32DataLen) {
		if (s_sRecData.u32PreAudLenInBuf) {
			if ((s_sRecData.u32ZeroAudLenInBuf == 0) &&
					(s_sRecData.u32PostAudLenInBuf)) {
				pu8TempBuf = s_sRecData.pu8PostAudTempBuf;
				pu32TempBufLen = &s_sRecData.u32PostAudLenInBuf;
			}
			else {
				pu8TempBuf = s_sRecData.pu8PreAudTempBuf;
				pu32TempBufLen = &s_sRecData.u32PreAudLenInBuf;
			}
		}
		else if (s_sRecData.u32ZeroAudLenInBuf) {
			pu8TempBuf = NULL;
			pu32TempBufLen = &s_sRecData.u32ZeroAudLenInBuf;
		}
		else {
			pu8TempBuf = s_sRecData.pu8PostAudTempBuf;
			pu32TempBufLen = &s_sRecData.u32PostAudLenInBuf;
		}

		u32TempBufLen = *pu32TempBufLen;
		u32CopyLen = u32DataLen - u32GetedLen;

		if (u32CopyLen > u32TempBufLen) {
			u32CopyLen = u32TempBufLen;
		}

		if (pu8TempBuf) {
			memcpy((pu8AudData + u32GetedLen),  pu8TempBuf, u32CopyLen);
			u32TempBufLen -= u32CopyLen;

			if (u32TempBufLen) {
				memcpy(pu8TempBuf, pu8TempBuf + u32CopyLen, u32TempBufLen);
			}

			*pu32TempBufLen = u32TempBufLen;
		}
		else {
			memset((pu8AudData + u32GetedLen),  0x0, u32CopyLen);
			u32TempBufLen -= u32CopyLen;
			*pu32TempBufLen = u32TempBufLen;
		}

		u32GetedLen += u32CopyLen;
	}

	if (u64CurTime < s_sRecData.u64FirstTimeStamp)
		u64CurTime = s_sRecData.u64FirstTimeStamp;

	double dSecondPerFrame;
	dSecondPerFrame = (double)(s_sRecData.u32OutFrameSize * 1000) / (double)(i32bps / 8);
	u32Frames = (uint32_t)((double)(u64CurTime - s_sRecData.u64FirstTimeStamp) / dSecondPerFrame);
//	u32Frames = ((uint32_t)(u64CurTime - s_sRecData.u64FirstTimeStamp))/(((s_sRecData.u32OutFrameSize *1000)/(i32bps/8)));
//	u32Frames ++;

	if (u32Frames == (s_sRecData.i32OutPrevFrames)) {
		u32Frames = (s_sRecData.i32OutPrevFrames + 1);
	}

	if (u32Frames < (s_sRecData.i32OutPrevFrames + 1)) {
//		DEBUGPRINT(": Discard audio data u32Frames %d, i32OutPrevFrames %d \n", u32Frames, s_sRecData.i32OutPrevFrames);
//		return false;
		u32Frames = (s_sRecData.i32OutPrevFrames + 1);
	}

	if (u32Frames > (s_sRecData.i32OutPrevFrames + 10)) {
		DEBUGPRINT(": u32Frames %d, i32OutPrevFrames %d \n", u32Frames, s_sRecData.i32OutPrevFrames);
		DEBUGPRINT(": u64CurTime %"PRId64"\n", u64CurTime);
		DEBUGPRINT(": s_sRecData.u64FirstTimeStamp %"PRId64"\n", s_sRecData.u64FirstTimeStamp);
		DEBUGPRINT(": s_sRecData.u32OutFrameSize %d\n", s_sRecData.u32OutFrameSize);
		DEBUGPRINT(": i32bps %d\n", i32bps);
		DEBUGPRINT(": u32TotalBytesInBuf %d\n", u32TotalBytesInBuf);
	}

	u64CurTime = s_sRecData.u64FirstTimeStamp + (uint64_t)(u32Frames * (((s_sRecData.u32OutFrameSize * 1000) / (i32bps / 8))));
	s_sRecData.i32OutPrevFrames = u32Frames;
	*pu64TimeStamp = u64CurTime;
	return true;
}



// ==================================================
// API implement
// ==================================================


ERRCODE
OSS_PlayOpen(
	S_NM_AUDIOCTX	*psCtx				// [in] Audio context.
) {
	int32_t i32DSPFD = -1;
	int32_t i32MixerFD = -1;
	int32_t i32OSS_Format = AFMT_S16_LE;
	int32_t i32Volume = 0x2020;
	int32_t i32DSPBlockSize;
	FUN_MUTEX_LOCK(&s_sOSSFunMutex);

	if (psCtx == NULL) {
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return ERR_OSS_CTX;
	}

	if (s_i32DSPFD > 0) {
		s_i32PlayRefCnt ++;
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		DEBUGPRINT("s_i32PlayRefCnt %d ++++++++++++++++++++\n", s_i32PlayRefCnt);
		return ERR_OSS_NONE;
	}

	if (psCtx->ePCMType != eNM_PCM_S16LE) {
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return ERR_OSS_CTX;
	}

	i32DSPFD = open(DEF_OSS_SPK, O_RDWR);

	if (i32DSPFD < 0) {
		ERRPRINT("open failed" DEF_OSS_SPK);
		i32DSPFD = open("/dev/dsp2", O_RDWR);

		if (i32DSPFD < 0) {
			ERRPRINT("open(/dev/dsp2) failed");
			FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
			return ERR_OSS_DEV;
		}
	} // if

	i32MixerFD = open("/dev/mixer", O_RDWR);

	if (i32MixerFD < 0) {
		ERRPRINT("open(/dev/mixer) failed");
		close(i32DSPFD);
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return ERR_OSS_DEV;
	} // if

	ioctl(i32DSPFD, SNDCTL_DSP_SETFMT, &i32OSS_Format);
	ioctl(i32MixerFD, MIXER_WRITE(SOUND_MIXER_VOLUME), &i32Volume);

	ioctl(i32DSPFD, SNDCTL_DSP_SPEED, &psCtx->u32SampleRate);
	ioctl(i32DSPFD, SNDCTL_DSP_CHANNELS, &psCtx->u32ChannelNum);
	ioctl(i32DSPFD, SNDCTL_DSP_GETBLKSIZE, &i32DSPBlockSize);

	s_i32DSPFD = i32DSPFD;
	s_i32MixerFD = i32MixerFD;
	s_i32DSPBlockSize = i32DSPBlockSize;
	g_u32NMPlayer_Volume = 0x20;
	s_i32PlayRefCnt ++;
	close(s_i32MixerFD);
	s_i32MixerFD = -1;
	FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
	return ERR_OSS_NONE;
}

uint32_t
OSS_PlayGetBlockSize(void) {
	return (uint32_t)s_i32DSPBlockSize;
}

uint32_t
OSS_PlayWrite(
	S_NM_AUDIOCTX	*psCtx,				// [in] Audio context.
	uint32_t		*pu32WrittenByte	// [out] Written bytes
) {
	audio_buf_info sAudBufinfo;
	fd_set sWritefds;
	int32_t i32DSPFD;
	int32_t i32DataWrite = 0;;
	int32_t i32DataWritten = 0;;
	FUN_MUTEX_LOCK(&s_sOSSFunMutex);

	if (s_i32DSPFD < 0) {
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return 0;
	}

	*pu32WrittenByte = 0;
	i32DSPFD = s_i32DSPFD;
	ioctl(i32DSPFD , SNDCTL_DSP_GETOSPACE , &sAudBufinfo);

	if (psCtx == NULL) {
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return sAudBufinfo.bytes;
	}

	if ((int32_t)psCtx->u32DataSize > s_i32DSPBlockSize) {
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);

		if (s_i32DSPBlockSize > sAudBufinfo.bytes)
			return sAudBufinfo.bytes;
		else
			return s_i32DSPBlockSize;
	}

	if (sAudBufinfo.bytes < (int32_t)psCtx->u32DataSize) {
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return sAudBufinfo.bytes;
	}
	
	i32DataWrite = psCtx->u32DataSize;
	FD_ZERO(&sWritefds);
	FD_SET(i32DSPFD , &sWritefds);
	select(i32DSPFD + 1 , NULL , &sWritefds , NULL, NULL);

	i32DataWritten = write(i32DSPFD, (char *)psCtx->pDataVAddr , i32DataWrite);
//	DEBUGPRINT("i32DataWritten %d\n", i32DataWritten);

	if (i32DataWritten < 0) {
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return 0;
	}

	*pu32WrittenByte = i32DataWritten;
	FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
	return sAudBufinfo.bytes;
}

ERRCODE
OSS_PlayGetVolume(
	uint32_t 		*pu32Volume		// [out] Current volume
) {
	int32_t i32Volume;

	if (pu32Volume == NULL)
		return ERR_OSS_NULL_POINTER;

	FUN_MUTEX_LOCK(&s_sOSSFunMutex);

	if (s_i32DSPFD < 0) {
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return ERR_OSS_DEV;
	}

	ioctl(s_i32MixerFD, MIXER_READ(SOUND_MIXER_VOLUME), &i32Volume);
	i32Volume = (i32Volume & 0x3f);
	*pu32Volume = i32Volume;
	FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
	return ERR_OSS_NONE;
}

ERRCODE
OSS_PlaySetVolume(
	uint32_t 		u32Volume			// [in] Set volume 0 ~ 0x3F
) {
	if ((int32_t)u32Volume < OSS_VOLUME_MIN)
		u32Volume = OSS_VOLUME_MIN;

	if (u32Volume > OSS_VOLUME_MAX)
		u32Volume = OSS_VOLUME_MAX;

	if (u32Volume == g_u32NMPlayer_Volume)
		return ERR_OSS_NONE;

	FUN_MUTEX_LOCK(&s_sOSSFunMutex);

	if (s_i32DSPFD < 0) {
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return ERR_OSS_DEV;
	}

	if (s_i32MixerFD < 0) {
		s_i32MixerFD = open("/dev/mixer", O_RDWR);

		if (s_i32MixerFD < 0) {
			ERRPRINT("open(/dev/mixer) failed");
			close(s_i32DSPFD);
			FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
			return ERR_OSS_DEV;
		} // if
	}

	if (s_i32MixerFD >= 0) {
		const int	i32Volume = (u32Volume << 8) | u32Volume;	// H/L: Right/Left

		if (ioctl(s_i32MixerFD, MIXER_WRITE(SOUND_MIXER_PCM), &i32Volume) < 0) {
			perror("ioctl(/dev/mixer, MIXER_WRITE(SOUND_MIXER_PCM)) failed");
			return ERR_OSS_VOLUME;
		} // if

		g_u32NMPlayer_Volume = u32Volume;
		close(s_i32MixerFD);
		s_i32MixerFD = -1;
	} // if

	FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
	return ERR_OSS_NONE;
}

ERRCODE
OSS_PlayClose(void) {

	FUN_MUTEX_LOCK(&s_sOSSFunMutex);

	if (s_i32DSPFD < 0) {
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return ERR_OSS_DEV;
	}

	s_i32PlayRefCnt--;

	if (s_i32PlayRefCnt < 0) {
		s_i32PlayRefCnt = 0;
	}
	else if (s_i32PlayRefCnt == 0) {
		close(s_i32DSPFD);
		close(s_i32MixerFD);
		s_i32DSPFD = -1;
		s_i32MixerFD = -1;
	}

	FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
	return ERR_OSS_NONE;
}

////////////////////////////////////////////////////////////////////////
//	Record
////////////////////////////////////////////////////////////////////////

ERRCODE
OSS_RecOpen(
	S_NM_AUDIOCTX	*psCtx				// [in] Audio context.
) {
	int32_t i32RecFD;
	int32_t i32Ret;
	int32_t i32Freg;
	uint32_t u32TempAudBufLen;
	E_KERNEL_VER eKernelVer = Util_GetKernelVer();

	FUN_MUTEX_LOCK(&s_sOSSFunMutex);

	if (psCtx == NULL) {
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return ERR_OSS_CTX;
	}

	if (s_sRecData.i32RecFD > 0) {
		DEBUGPRINT(DEF_OSS_MIC "had been opened \n");
		psCtx->u32DataSize = s_sRecData.u32OutFrameSize;
		psCtx->u32DataLimit = s_sRecData.u32OutFrameSize;
		psCtx->u32SampleNum = s_sRecData.u32OutFrameSize / psCtx->u32ChannelNum / 2;  //2:PCM_S16LE
		s_i32RecRefCnt ++;
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return ERR_OSS_NONE;
	}

	i32RecFD = open(DEF_OSS_MIC, O_RDWR);

	if (i32RecFD < 0) {
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return ERR_OSS_DEV;
	}

	i32Ret = ioctl(i32RecFD, SNDCTL_DSP_GETBLKSIZE, &i32Freg);

	if (i32Ret < 0)
		s_sRecData.u32DrvFrameSize = REC_BLOCK_SIZE;
	else
		s_sRecData.u32DrvFrameSize = i32Freg;

	if(eKernelVer != eKERNEL_2_6_17_14){
		int format = AFMT_S16_LE;
		i32Ret = ioctl(i32RecFD, SNDCTL_DSP_SETFMT, &format);
	}
	else{
		i32Ret = ioctl(i32RecFD, SNDCTL_DSP_SETFMT, AFMT_S16_LE);
	}

	i32Ret = ioctl(i32RecFD, SNDCTL_DSP_CHANNELS, &(psCtx->u32ChannelNum));

	if (i32Ret < 0) {
		close(i32RecFD);
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return ERR_OSS_CTX;
	}

	i32Ret = ioctl(i32RecFD, SNDCTL_DSP_SPEED, &(psCtx->u32SampleRate));

	if (i32Ret < 0) {
		close(i32RecFD);
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return ERR_OSS_CTX;
	}

	fcntl(i32RecFD, F_SETFL, O_NONBLOCK);

	s_sRecData.u32OutFrameSize = psCtx->u32SampleRate;
	s_sRecData.pu8RecvAudBuf = Util_Malloc(s_sRecData.u32DrvFrameSize);

	if (s_sRecData.pu8RecvAudBuf == NULL) {
		close(i32RecFD);
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return ERR_OSS_MALLOC;
	}

	if (s_sRecData.u32OutFrameSize > s_sRecData.u32DrvFrameSize)
		u32TempAudBufLen = s_sRecData.u32OutFrameSize;
	else
		u32TempAudBufLen = s_sRecData.u32DrvFrameSize;

	s_sRecData.pu8PreAudTempBuf = Util_Malloc(u32TempAudBufLen * REC_TEMP_BUF_BLOCK);

	if (s_sRecData.pu8PreAudTempBuf == NULL) {
		Util_Free(s_sRecData.pu8RecvAudBuf, s_sRecData.u32DrvFrameSize);
		s_sRecData.pu8RecvAudBuf = NULL;
		close(i32RecFD);
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return ERR_OSS_MALLOC;
	}

	s_sRecData.u32PreAudLenInBuf = 0;
	s_sRecData.u32PreAudTempBufSize = u32TempAudBufLen * REC_TEMP_BUF_BLOCK;
	s_sRecData.pu8PostAudTempBuf = Util_Malloc(u32TempAudBufLen * REC_TEMP_BUF_BLOCK);

	if (s_sRecData.pu8PostAudTempBuf == NULL) {
		Util_Free(s_sRecData.pu8RecvAudBuf, s_sRecData.u32DrvFrameSize);
		s_sRecData.pu8RecvAudBuf = NULL;
		Util_Free(s_sRecData.pu8PreAudTempBuf, s_sRecData.u32PreAudTempBufSize);
		s_sRecData.pu8PreAudTempBuf = NULL;
		close(i32RecFD);
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return ERR_OSS_MALLOC;
	}

	s_sRecData.u32PostAudLenInBuf = 0;
	s_sRecData.u32PostAudTempBufSize = u32TempAudBufLen * REC_TEMP_BUF_BLOCK;

	if(eKernelVer != eKERNEL_2_6_17_14){
		uint8_t	au8Tmp[16];
	
		read(i32RecFD, au8Tmp, sizeof(au8Tmp));
	}

	s_sRecData.u32ZeroAudLenInBuf = 0;
	s_sRecData.u64FirstTimeStamp = 0;
	s_sRecData.i32PrevFrames = -1;
	s_sRecData.i32OutPrevFrames = -1;
	DEBUGPRINT("OSS fragment size %d, App frames size %d\n", s_sRecData.u32DrvFrameSize, s_sRecData.u32OutFrameSize);
	s_sRecData.i32RecFD =  i32RecFD;
	psCtx->u32DataSize = s_sRecData.u32OutFrameSize;
	psCtx->u32DataLimit = s_sRecData.u32OutFrameSize;
	psCtx->u32SampleNum = s_sRecData.u32OutFrameSize / psCtx->u32ChannelNum / 2;  //2:PCM_S16LE
	s_i32RecRefCnt ++;
	FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
	return ERR_OSS_NONE;
}

static uint64_t s_u64DBGOrigTime;
static uint64_t s_u64DBGModifyTime;

ERRCODE
OSS_RecRead(
	S_NM_AUDIOCTX	*psCtx				// [in] Audio context.
) {
	int32_t i32Ret;
	int32_t i32bps = 0;
	uint64_t u64CurTime;
	int32_t i32BDelay;
	int32_t i32Frames;
	uint64_t u64CurAudioTS = 0;
	E_KERNEL_VER eKernelVer = Util_GetKernelVer();

	FUN_MUTEX_LOCK(&s_sOSSFunMutex);
	psCtx->u32DataSize = 0;

	if (s_sRecData.i32RecFD < 0) {
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return ERR_OSS_DEV;
	}

	i32bps = psCtx->u32SampleRate * 16 * psCtx->u32ChannelNum;

	if(eKernelVer != eKERNEL_2_6_17_14){
		audio_buf_info bi;
	
		ioctl(s_sRecData.i32RecFD, SNDCTL_DSP_GETISPACE, &bi);
	
		if(bi.bytes < s_sRecData.u32DrvFrameSize){
			FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
			return ERR_OSS_IO;
		}
	}

	i32Ret = read(s_sRecData.i32RecFD, s_sRecData.pu8RecvAudBuf, s_sRecData.u32DrvFrameSize);

	if (i32Ret == 0) {
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return ERR_OSS_IO;
	}

	if (i32Ret < 0) {
		if ((s_sRecData.u32PreAudLenInBuf + s_sRecData.u32ZeroAudLenInBuf + s_sRecData.u32PostAudLenInBuf) < s_sRecData.u32OutFrameSize) {
			FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
			return ERR_OSS_NOT_READY;
		}
		else {
			goto get_audio;
		}
	}

	double dBDelayTime;
	u64CurTime = Util_GetTime();
	s_u64DBGOrigTime = u64CurTime;
	i32BDelay = i32Ret / (16 / 8);
	dBDelayTime = (double)(i32BDelay * 1000) / (double)(psCtx->u32SampleRate * psCtx->u32ChannelNum);
	u64CurTime -= (uint64_t) dBDelayTime;
//    u64CurTime -= (uint64_t)(((i32BDelay*1000)/(psCtx->u32SampleRate*psCtx->u32ChannelNum)));
	s_u64DBGModifyTime = u64CurTime;

	if (s_sRecData.u64FirstTimeStamp == 0) {
		s_sRecData.u64FirstTimeStamp = u64CurTime;
		s_sRecData.u64NextTimeStamp = u64CurTime;
	}

	double dSecondPerFrame;
	dSecondPerFrame = (double)(s_sRecData.u32DrvFrameSize * 1000) / (double)(i32bps / 8);
	i32Frames = (uint32_t)((double)(u64CurTime - s_sRecData.u64FirstTimeStamp) / dSecondPerFrame);
//	i32Frames = ((uint32_t)(u64CurTime - s_sRecData.u64FirstTimeStamp)) / (((s_sRecData.u32DrvFrameSize*1000)/(i32bps/8)));

	if (i32Frames == (s_sRecData.i32PrevFrames)) {
		i32Frames = (s_sRecData.i32PrevFrames + 1);
	}

	if (i32Frames >= (s_sRecData.i32PrevFrames + 1)) {
		if ((i32Frames - (s_sRecData.i32PrevFrames)) > 1) { //lost audio data, fill 0 into buffer
			DEBUGPRINT("Aud Frames %d,  Prev frames %d\n", i32Frames, s_sRecData.i32PrevFrames);
			DEBUGPRINT("Aud s_u32DBGOrigTime %"PRId64", s_u32DBGModifyTime %"PRId64"\n", s_u64DBGOrigTime, s_u64DBGModifyTime);

			if (s_sRecData.u32PostAudLenInBuf) {
				DEBUGPRINT("%s \n", ": Skip zero data");
				s_sRecData.i32PrevFrames = i32Frames;
				goto get_audio;
			}

			s_sRecData.u32ZeroAudLenInBuf = (i32Frames - (s_sRecData.i32PrevFrames - 1)) * s_sRecData.u32DrvFrameSize;
		}

		s_sRecData.i32PrevFrames = i32Frames;
		// put data into buffer
		PutAudioToBuf(s_sRecData.pu8RecvAudBuf, i32Ret);
	}
	else {
		DEBUGPRINT(": discard audio data %d %d %"PRId64" \n", i32Frames, s_sRecData.i32PrevFrames, u64CurTime);
	}

get_audio:

	if (GetAudioFromBuf((uint8_t *)psCtx->pDataVAddr , s_sRecData.u32OutFrameSize, &u64CurAudioTS,
						psCtx->u32ChannelNum, psCtx->u32SampleRate) == false) {
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return ERR_OSS_NOT_READY;
	}

	psCtx->u64DataTime = u64CurAudioTS;
	psCtx->u32DataSize = s_sRecData.u32OutFrameSize;
	FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
	return ERR_OSS_NONE;
}


ERRCODE
OSS_RecClose(void) {


	FUN_MUTEX_LOCK(&s_sOSSFunMutex);

	if (s_sRecData.i32RecFD < 0) {
		FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
		return ERR_OSS_DEV;
	}

	s_i32RecRefCnt --;

	if (s_i32RecRefCnt < 0) {
		s_i32RecRefCnt = 0;
	}
	else if (s_i32RecRefCnt == 0) {
		if (s_sRecData.pu8RecvAudBuf) {
			Util_Free(s_sRecData.pu8RecvAudBuf, s_sRecData.u32DrvFrameSize);
			s_sRecData.pu8RecvAudBuf = NULL;
		}

		if (s_sRecData.pu8PreAudTempBuf) {
			Util_Free(s_sRecData.pu8PreAudTempBuf, s_sRecData.u32PreAudTempBufSize);
			s_sRecData.pu8PreAudTempBuf = NULL;
		}

		if (s_sRecData.pu8PostAudTempBuf) {
			Util_Free(s_sRecData.pu8PostAudTempBuf, s_sRecData.u32PostAudTempBufSize);
			s_sRecData.pu8PostAudTempBuf = NULL;
		}

		close(s_sRecData.i32RecFD);
		s_sRecData.i32RecFD = -1;
	}

	FUN_MUTEX_UNLOCK(&s_sOSSFunMutex);
	return ERR_OSS_NONE;
}

uint32_t
OSS_RecGetBlockSize(void) {
	return s_sRecData.u32DrvFrameSize;
}


void
OSS_RecSetFrameSize(
	uint32_t u32FrameSize
) {
	s_sRecData.u32OutFrameSize = u32FrameSize;
}



