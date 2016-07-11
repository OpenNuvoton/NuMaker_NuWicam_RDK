//------------------------------------------------------------------
//
// Copyright (c) Nuvoton Technology Corp. All rights reserved.
//
//------------------------------------------------------------------

#ifndef _OSS_H__
#define _OSS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OSS_VOLUME_MIN	0
#define OSS_VOLUME_MAX	100

#ifndef ERRCODE
#define ERRCODE int32_t
#endif

#define OSS_ERR							0x80000000
#define ERR_OSS_NONE						0
#define ERR_OSS_NULL_POINTER			(OSS_ERR | 0x01)
#define ERR_OSS_MALLOC					(OSS_ERR | 0x02)
#define ERR_OSS_CTX						(OSS_ERR | 0x03)
#define ERR_OSS_RES						(OSS_ERR | 0x04)
#define ERR_OSS_DEV						(OSS_ERR | 0x04)
#define ERR_OSS_VOLUME					(OSS_ERR | 0x04)
#define ERR_OSS_IO						(OSS_ERR | 0x04)
#define ERR_OSS_NOT_READY				(OSS_ERR | 0x04)

// PCM type
typedef enum _E_NM_PCMTYPE {
	eNM_PCM_S16LE	= 0x00,				// Signed 16-bit little-endian
	eNM_PCM_TYPENUM						// Number of PCM type
} E_NM_PCMTYPE;

// ==================================================
// Audio context
// (Define audio source/input or output/record)
// ==================================================
typedef struct _S_NM_AUDIOCTX {
    void			*pDataVAddr;		// Start virtual address of audio data
    void			*pDataPAddr;		// Start physical address of audio data
	uint32_t		u32DataSize;		// Actual byte size of audio data
	uint32_t		u32DataLimit;		// Byte size limit to store audio data
	uint64_t		u64DataTime;		// Millisecond offset of audio data

	E_NM_PCMTYPE	ePCMType;			// PCM type of raw audio data
	uint32_t		u32BitRate;			// bps
	uint32_t		u32ChannelNum;		// > 0
	uint32_t		u32SampleNum;		// Number of sample in context
										// (E.g. 1 sample has 1L + 1R in stereo)
	uint32_t		u32SampleRate;		// Hz

} S_NM_AUDIOCTX;

#define FUN_MUTEX_LOCK(x)		pthread_mutex_lock(x)
#define FUN_MUTEX_UNLOCK(x)		pthread_mutex_unlock(x)

#define Util_Malloc(x)	malloc(x)
#define Util_Free(x,y)	free(x)
#define Util_Calloc(x, y)	calloc(x, y)

uint64_t
Util_GetTime(void);

#if !defined (DEBUGPRINT)
//#undef DEBUG
/* DEBUG MACRO */
#define WHERESTR                                        "[%-20s %-4u {PID:%-4d} %"PRId64"] "
#define WHEREARG                                        __FUNCTION__, __LINE__, getpid(), Util_GetTime()
#define DEBUGPRINT2(...)                fprintf(stderr, ##__VA_ARGS__)
//#ifdef DEBUG
        #define DEBUGPRINT(_fmt, ...)   DEBUGPRINT2(WHERESTR _fmt, WHEREARG, ##__VA_ARGS__)
//#else
 //       #define DEBUGPRINT(_fmt, ...)
//#endif
#endif


#define ERRPRINT(_fmt, ...)   fprintf(stderr, "[%-20s %-4u {PID:%-4d} %"PRId64"]" _fmt,  __FUNCTION__, __LINE__, getpid(), Util_GetTime(), ##__VA_ARGS__)


// ==================================================
// API declaration
// ==================================================

	ERRCODE
	OSS_PlayOpen(
		S_NM_AUDIOCTX	*psCtx				// [in] Audio context.
	);

	uint32_t
	OSS_PlayWrite(
		S_NM_AUDIOCTX	*psCtx,				// [in] Audio context.
		uint32_t		*pu32WrittenByte	// [out] Written bytes
	);

	ERRCODE
	OSS_PlayGetVolume(
		uint32_t 		*pu32Volume		// [out] Current volume
	);

	ERRCODE
	OSS_PlaySetVolume(
		uint32_t 		u32Volume//,			// [in] Set volume
	);


	ERRCODE
	OSS_PlayClose(void);


	ERRCODE
	OSS_RecOpen(
		S_NM_AUDIOCTX	*psCtx			// [in/out] Audio context.
		// [in] psCtx->u32ChannelNum, psCtx->u32SampleRate, psCtx->ePCMType
		// [out] psCtx->u32DataLimit: Output frame size, psCtx->u32SampleNum: Sample number in one frame
	);

	ERRCODE
	OSS_RecRead(
		S_NM_AUDIOCTX	*psCtx				// [in] Audio context.
	);

	ERRCODE
	OSS_RecClose(void);


	uint32_t				//in bytes
	OSS_PlayGetBlockSize(void);

	uint32_t				//in bytes
	OSS_RecGetBlockSize(void);

	void
	OSS_RecSetFrameSize(
		uint32_t u32FrameSize	//in bytes
	);

#ifdef __cplusplus
} // extern "C"
#endif

#endif //__CTX_OSS_H__
