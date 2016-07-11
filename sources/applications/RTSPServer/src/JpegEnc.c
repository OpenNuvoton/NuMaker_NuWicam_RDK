/* JpegEnc.c
 *
 *
 * Copyright (c)2008 Nuvoton technology corporation
 * http://www.nuvoton.com
 *
 * JPEG encode function
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/ioctl.h>

//#include <linux/videodev.h>
#include "JpegEnc.h"


static uint8_t *s_pu8JpegEncBuf = MAP_FAILED;
static int32_t s_i32JpegFd = -1;
static uint32_t s_u32JpegBufSize;
static jpeg_param_t s_sJpegParam;
static jpeg_info_t *s_pJpegInfo = NULL;
static uint32_t s_u32JpegInfoSize = 0;	

#define DBG_POINT()		printf("Function:%s. Line: %d\n", __FUNCTION__, __LINE__);
#define MAX_JPEG_BUF_CNT 1
#define RETRY_NUM 4

#define MAX_Q_SCALE		15
#define DEF_Q_SCALE		15
#define INVALID_FRAME_SIZE 0xFFFFFFFF
#define DEF_BIT_RATE	3000000

//#define DEF_FIX_QUALITY	15

static uint32_t s_au32BRCTable[MAX_Q_SCALE + 1];	//Bit rate control table
static uint32_t s_u32QScale;


// Init BRC table
bool
BRCInitTable(
	uint32_t u32QScale,
	uint32_t u32FrameSize
)
{
	if (u32QScale > MAX_Q_SCALE)
		return false;

	s_au32BRCTable[u32QScale] = u32FrameSize;
	return true;
}

bool
BRCUpdateTable(
	uint32_t u32QScale,
	uint32_t u32FrameSize
)
{
	uint32_t *pau32CRCTable;

	if (u32QScale > MAX_Q_SCALE)
		return false;

	pau32CRCTable = &(s_au32BRCTable[0]);
	pau32CRCTable[u32QScale] = u32FrameSize;

	if (u32QScale == MAX_Q_SCALE) {
		pau32CRCTable[MAX_Q_SCALE - 1] = (pau32CRCTable[MAX_Q_SCALE - 2] + u32FrameSize) / 2;
	}
	else if (u32QScale == (MAX_Q_SCALE - 1)) {
		pau32CRCTable[MAX_Q_SCALE] = (pau32CRCTable[MAX_Q_SCALE] + u32FrameSize) / 2;
		pau32CRCTable[u32QScale - 1] = (pau32CRCTable[u32QScale - 2] + u32FrameSize) / 2;
	}
	else if (u32QScale == 1) {
		pau32CRCTable[2] = (pau32CRCTable[3] + u32FrameSize) / 2;
	}
	else if (u32QScale == 2) {
		pau32CRCTable[1] = (pau32CRCTable[1] + u32FrameSize) / 2;
		pau32CRCTable[3] = (pau32CRCTable[4] + u32FrameSize) / 2;
	}
	else {
		pau32CRCTable[u32QScale + 1] = (pau32CRCTable[u32QScale + 2] + u32FrameSize) / 2;
		pau32CRCTable[u32QScale - 1] = (pau32CRCTable[u32QScale - 2] + u32FrameSize) / 2;
	}

	return true;
}

void
BRCGetQScale(
	uint32_t u32BitRate,
	uint32_t u32FrameRate,
	uint32_t *pu32QScale
)
{
	uint32_t u32FrameSize;
	uint32_t i;
	u32FrameSize = (u32BitRate / 8) / u32FrameRate;

	for (i = MAX_Q_SCALE ; i > 0; i --) {
		if (s_au32BRCTable[i] > u32FrameSize)
			break;
	}

	if ((i > 1) && (s_au32BRCTable[i - 1] == INVALID_FRAME_SIZE))
		s_au32BRCTable[i - 1] = u32FrameSize;

	if ((i < MAX_Q_SCALE))
		i++;

	*pu32QScale = i;
}


static int32_t
ReadJpegEncInfo(
	int32_t i32JpegFD,
	jpeg_info_t *psJpegInfo,
	uint32_t u32JpegInfoSize,
	uint32_t u32Timeout
)
{
#if 0
	int32_t i32Ret = -1;
	uint64_t u64EndTime;
	
	//For speed up JPEG encoding performance
	//Add delay_once variable.
	int delay_once=1;
	
	if(u32Timeout == 0){
		u32Timeout = 200;
	}
	
	fcntl(i32JpegFD, F_SETFL, O_NONBLOCK);

	u64EndTime = Util_GetTime() + (uint64_t)u32Timeout;
	while(Util_GetTime() < u64EndTime){
		i32Ret = read(i32JpegFD, psJpegInfo, u32JpegInfoSize);
		if(i32Ret >= 0)
			break;
		if(errno != EAGAIN){
			printf(": Jpeg read fail (%s)\n", strerror(errno));
			break;
		}
		if(delay_once) usleep(10);
		delay_once=0;
	}

	return i32Ret;
#else
	return read(i32JpegFD, psJpegInfo, u32JpegInfoSize);
#endif
}


int 
InitJpegDevice(void)
{
	int32_t i;
	char achDevice[] = "/dev/video1";

	for(i=0; i < RETRY_NUM; i++){
		s_i32JpegFd = open(achDevice, O_RDWR);         //maybe /dev/video1, video2, ...
		if (s_i32JpegFd  < 0){
			printf("open %s error\n", achDevice);
			achDevice[10]++;
			continue;       
		}
		/* allocate memory for JPEG engine */
		if((ioctl(s_i32JpegFd, JPEG_GET_JPEG_BUFFER, &s_u32JpegBufSize)) < 0){
			close(s_i32JpegFd);
			achDevice[10]++;
			printf("Try to open %s\n",achDevice);
		}
		else{       
			printf("jpegcodec is %s\n",achDevice);
			break;                  
		}
	}
	if(s_i32JpegFd < 0)
		return s_i32JpegFd; 

	s_u32JpegInfoSize = sizeof(jpeg_info_t) + (MAX_JPEG_BUF_CNT * sizeof(__u32));
	s_pJpegInfo = malloc(s_u32JpegInfoSize);
	if(s_pJpegInfo == NULL){
		printf("Allocate JPEG info failed\n");
		close(s_i32JpegFd);
		return -1;		
	}

	s_pu8JpegEncBuf = mmap(NULL, s_u32JpegBufSize, PROT_READ|PROT_WRITE, MAP_SHARED, s_i32JpegFd, 0);
	
	if(s_pu8JpegEncBuf == MAP_FAILED){
		printf("JPEG Map Failed!\n");
		close(s_i32JpegFd);
		return -1;
	}

	for (i = 1; i <= MAX_Q_SCALE; i++)
		BRCInitTable(i, INVALID_FRAME_SIZE);

	memset((void*)&s_sJpegParam, 0, sizeof(s_sJpegParam));


	s_sJpegParam.encode = 1;						/* Encode */
	s_sJpegParam.encode_image_format = DRVJPEG_ENC_PRIMARY_YUV420;

	s_sJpegParam.buffersize = 0;		/* only for continuous shot */
	s_sJpegParam.buffercount = 1;		

	s_sJpegParam.scale = 0;		/* Scale function is disabled when scale 0 */	
	s_sJpegParam.qscaling = 10;			
	s_sJpegParam.qadjust = 0xf;			
	printf("JPEG file descriptor = %d\n", s_i32JpegFd);
	return s_i32JpegFd;
}

int
JpegEnc(
	S_NM_VIDEOCTX *psVideoSrcCtx,
	S_NM_VIDEOCTX *psVideoDstCtx
)
{
	jpeg_param_t *psJpegParam = &s_sJpegParam;
	
	memset(psJpegParam, 0x00, sizeof(jpeg_param_t));
	psJpegParam->encode = 1;    // 1:encode
	psJpegParam->buffersize = 0;
	psJpegParam->buffercount = MAX_JPEG_BUF_CNT;

	if (psVideoSrcCtx->pDataPAddr) {
//		psJpegParam->src_bufsize = 0;
		psJpegParam->src_bufsize = 1024;
	}
	else {
		if((psVideoSrcCtx->u32DataSize + 300000) > s_u32JpegBufSize){
			printf ("JPEG driver buffer is not enough\n");
			return -1;
		}
		
		psJpegParam->src_bufsize = psVideoSrcCtx->u32DataSize; //set raw image size
	}

	psJpegParam->dst_bufsize = s_u32JpegBufSize - psJpegParam->src_bufsize;
	psJpegParam->decInWait_buffer_size = 0;

	if (psVideoDstCtx->eColorType == eNM_COLOR_YUV422P) {
		psJpegParam->encode_source_format  = DRVJPEG_ENC_SRC_PLANAR;
		psJpegParam->encode_image_format  = DRVJPEG_ENC_PRIMARY_YUV422;
	}
	else if (psVideoDstCtx->eColorType == eNM_COLOR_YUV420P) {
		psJpegParam->encode_source_format  = DRVJPEG_ENC_SRC_PLANAR;
		psJpegParam->encode_image_format  = DRVJPEG_ENC_PRIMARY_YUV420;
	}
	else {
		psJpegParam->encode_source_format  = DRVJPEG_ENC_SRC_PACKET;

		if (psVideoDstCtx->eColorType == eNM_COLOR_YUV420)
			psJpegParam->encode_image_format  = DRVJPEG_ENC_PRIMARY_YUV420;
		else
			psJpegParam->encode_image_format  = DRVJPEG_ENC_PRIMARY_YUV422;
	}

	psJpegParam->qadjust = 0xf;

#if defined (DEF_FIX_QUALITY)
	s_u32QScale = DEF_FIX_QUALITY;
#else	
	BRCGetQScale(psVideoDstCtx->u32BitRate, psVideoDstCtx->u32FrameRate, &s_u32QScale);
#endif
	psJpegParam->qscaling  = s_u32QScale;
	
	if((psVideoSrcCtx->u32Width == psVideoDstCtx->u32Width) && (psVideoSrcCtx->u32Height == psVideoDstCtx->u32Height)){
		psJpegParam->scale = 0;
	}
	else{
		psJpegParam->scale = 1;
	}

	psJpegParam->encode_width = psVideoSrcCtx->u32Width;
	psJpegParam->encode_height = psVideoSrcCtx->u32Height;
	psJpegParam->scaled_width = psVideoDstCtx->u32Width;
	psJpegParam->scaled_height = psVideoDstCtx->u32Height;

	if (psVideoSrcCtx->pDataPAddr) {
		psJpegParam->vaddr_src = (uint32_t) psVideoSrcCtx->pDataPAddr;
		psJpegParam->paddr_src = (uint32_t) psVideoSrcCtx->pDataPAddr;
		
		if(psVideoSrcCtx->u32StrideW > psVideoSrcCtx->u32Width){
			ioctl(s_i32JpegFd, JPEG_SET_ENC_STRIDE, psVideoSrcCtx->u32StrideW);
		}
	}
	else {
		if (psVideoSrcCtx->u32StrideW > psVideoSrcCtx->u32Width) {
			uint32_t u32DrawX, u32DrawY;
			uint8_t *pu8SrcImgYAdr;
			uint8_t *pu8DestImgYAdr;
			uint32_t Y;
			pu8SrcImgYAdr = psVideoSrcCtx->pDataVAddr;
			pu8DestImgYAdr = s_pu8JpegEncBuf;
			u32DrawY = psVideoDstCtx->u32Height;
			u32DrawX = psVideoDstCtx->u32Width;

			for (Y = 0; Y < u32DrawY; Y++) {
				memcpy(pu8DestImgYAdr, pu8SrcImgYAdr, u32DrawX * 2);
				pu8SrcImgYAdr += psVideoSrcCtx->u32StrideW * 2;
				pu8DestImgYAdr += u32DrawX * 2;
			}
		}
		else {
			memcpy(s_pu8JpegEncBuf, (void *)psVideoSrcCtx->pDataVAddr, psVideoSrcCtx->u32DataSize);
		}
	}
	ioctl(s_i32JpegFd, JPEG_S_PARAM, psJpegParam);
	ioctl(s_i32JpegFd, JPEG_TRIGGER, NULL);

#if 0
	if (ReadJpegEncInfo(s_i32JpegFd, s_pJpegInfo, s_u32JpegInfoSize, 200) < 0) {
		printf("%s", ": Jpeg read fail\n");
		return -1;
	}

	ioctl(s_i32JpegFd, JPEG_FLUSH_CACHE, 0);

	psVideoDstCtx->pDataVAddr = s_pu8JpegEncBuf + psJpegParam->src_bufsize;
	psVideoDstCtx->u32DataSize = s_pJpegInfo->image_size[0];
	BRCUpdateTable(s_u32QScale, psVideoDstCtx->u32DataSize);

	return psVideoDstCtx->u32DataSize; 
#else
	return 0;
#endif
}

int
WaitJpegEncDone(
	S_NM_VIDEOCTX *psVideoDstCtx
)
{
	jpeg_param_t *psJpegParam = &s_sJpegParam;

	if (ReadJpegEncInfo(s_i32JpegFd, s_pJpegInfo, s_u32JpegInfoSize, 200) < 0) {
		printf("%s", ": Jpeg read fail\n");
		return -1;
	}

	ioctl(s_i32JpegFd, JPEG_FLUSH_CACHE, 0);

	psVideoDstCtx->pDataVAddr = s_pu8JpegEncBuf + psJpegParam->src_bufsize;
	psVideoDstCtx->u32DataSize = s_pJpegInfo->image_size[0];
	BRCUpdateTable(s_u32QScale, psVideoDstCtx->u32DataSize);

	return psVideoDstCtx->u32DataSize; 
}

int 
FinializeJpegDevice(void)
{
	if(s_pJpegInfo){
		free(s_pJpegInfo);
		s_pJpegInfo = NULL;
	}

	if(s_pu8JpegEncBuf !=  MAP_FAILED){
		munmap(s_pu8JpegEncBuf, s_u32JpegBufSize);
		s_pu8JpegEncBuf = MAP_FAILED;	
	}

	if(s_i32JpegFd > 0){
		close(s_i32JpegFd);
		s_i32JpegFd = -1;
	}
	return 0;
}

