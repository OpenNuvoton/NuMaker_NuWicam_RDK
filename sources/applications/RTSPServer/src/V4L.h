/* V4L.h
 *
 *
 * Copyright (c)2008 Nuvoton technology corporation
 * http://www.nuvoton.com
 *
 * V4L header file
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef __V4L_H__
#define __V4L_H__

#include <stdint.h>

// Defined Error Code in here
#ifndef ERRCODE
#define ERRCODE int32_t
#endif

typedef struct{
	unsigned int u32RemainBufSize;
	unsigned int u32RemainBufPhyAdr;
}S_BUF_INFO;

typedef struct 
{
	int32_t i32PipeBufNo;
	int32_t i32PipeBufSize;
	int32_t i32CurrPipePhyAddr;
}S_PIPE_INFO;

#define V4L_ERR					0x80400000							
#define ERR_V4L_SUCCESS				0
#define ERR_V4L_OPEN_DEV			(V4L_ERR | 0x1)
#define ERR_V4L_VID_GET_CAP			(V4L_ERR | 0x2)
#define ERR_V4L_VID_GRAB_DEV			(V4L_ERR | 0x3)
#define ERR_V4L_VID_SET_PICT			(V4L_ERR | 0x4)
#define ERR_V4L_VID_SET_WIN			(V4L_ERR | 0x5)
#define ERR_AVDEV_VID_CAPTURE			(V4L_ERR | 0x6)
#define ERR_V4L_MMAP				(V4L_ERR | 0x7)
#define ERR_V4L_VID_MCAPTURE			(V4L_ERR | 0x8)
#define ERR_V4L_VID_SYNC			(V4L_ERR | 0x9)
#define ERR_V4L_VID_CAPTURE			(V4L_ERR | 0xA)
#define ERR_V4L_VID_QUERY_UID		(V4L_ERR | 0xB)


/* Sensor CTRL */
#define REG_SDE_CTRL		0x6A
#define REG_BRIGHT		0x9B


#define VIDIOCGCAPTIME				_IOR('v',30,struct v4l2_buffer)      /*Get Capture time */
#define VIDIOCSPREVIEW  			_IOR('v',43, int)
#define VIDIOC_QUERY_SENSOR_ID		_IOR('v',51, int)
#define	VIDIOC_G_BUFINFO			_IOR('v',52, S_BUF_INFO)

#define VIDIOC_G_PACKET_INFO		_IOR('v',53, S_PIPE_INFO)			/* Get Packet offset address and size */ 
#define VIDIOC_G_PLANAR_INFO		_IOR('v',54, S_PIPE_INFO)			/* Get Planar offset address and size */ 
#define VIDIOC_S_MAP_BUF			_IOW('v',55, int)					/* Specified the mapping buffer */

#define VIDEO_START	0
#define VIDEO_STOP	1

typedef enum E_NM_COLORTYPE {
	eNM_COLOR_YUV420		= 0x00,						///< 16 bpp, YUV420-packet
	eNM_COLOR_YUV422		= 0x01,						///< 16 bpp, YUV422-packet, YCbYCr
	eNM_COLOR_YUV420P		= 0x08 | eNM_COLOR_YUV420,	///< 16 bpp, YUV420-planar
	eNM_COLOR_YUV422P		= 0x08 | eNM_COLOR_YUV422,	///< 16 bpp, YUV422-planar
	eNM_COLOR_YUV420P_MB	= 0x04 | eNM_COLOR_YUV420P,	///< 16 bpp, YUV420-planar, H.264 macro block
	eNM_COLOR_RGB565		= 0x80 | 0x00,				///< 16 bpp
} E_NM_COLORTYPE;

typedef struct S_NM_VIDEOCTX {
	// 1D presentation
	void			*pDataVAddr;		///< Start virtual address of video data
	void			*pDataPAddr;		///< Start physical address of video data
	uint32_t		u32DataSize;		///< Actual byte size of video data
	uint32_t		u32DataLimit;		///< Byte size limit to store video data
	uint64_t		u64DataTime;		///< Millisecond timestamp of video data

	E_NM_COLORTYPE	eColorType;			///< Color type of raw video data
	uint32_t		u32BitRate;			///< bps
	uint32_t		u32FrameRate;			///< fps

	// 2D presentation
	uint32_t		u32Width;			///< Pixel, > 0
	uint32_t		u32Height;			///< Pixel, > 0
	uint32_t		u32StrideW;			///< Pixel, u32StrideW must >= u32Width
	uint32_t		u32StrideH;			///< Pixel, u32StrideH must >= u32Height
	uint32_t		u32X;				///< Pixel, > 0 (ONLY used for cropping/clipping window)
	uint32_t		u32Y;				///< Pixel, > 0 (ONLY used for cropping/clipping window)
} S_NM_VIDEOCTX;

ERRCODE
InitV4LDevice(
	S_NM_VIDEOCTX *psVideoSrcCtx
);

ERRCODE
ReadV4LPicture(
	S_NM_VIDEOCTX *psVideoSrcCtx
);

ERRCODE
TriggerV4LNextFrame(void);

void FinializeV4LDevice();

ERRCODE
StartV4LCapture(void);

ERRCODE
StopV4LCapture(void);

ERRCODE
SetV4LViewWindow(int lcmwidth, int lcmheight, int prewidth, int preheight);

ERRCODE
StartPreview(void);

ERRCODE
StopPreview(void);

int dumpV4LBuffer(void);
#endif

