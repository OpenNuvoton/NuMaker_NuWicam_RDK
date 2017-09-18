/****************************************************************
 *                                                              *
 * Copyright (c) Nuvoton Technology Corp.  All rights reserved. *
 *                                                              *
 ****************************************************************/
#ifndef __IMG_PROC_H__
#define __IMG_PROC_H__

#include <stdint.h>

#include "ImgProc_Comm.h"

#define ERR_IMGPROC_SUCCESS						0
#define ERR_IMGPROC_NOT_SUPP				(IMGPROC_ERR | 0x1)
#define ERR_IMGPROC_BAD_ATTR				(IMGPROC_ERR | 0x2)
#define ERR_IMGPROC_RES						(IMGPROC_ERR | 0x3)
#define ERR_IMGPROC_MALLOC					(IMGPROC_ERR | 0x4)
#define ERR_IMGPROC_COLOR_FORMAT			(IMGPROC_ERR | 0x5)

typedef enum {
	eIMGPROC_OP_SW,
	eIMGPROC_OP_HW,
} E_IMGPROC_OP_MODE;


typedef enum {
	eIMGPROC_RGB888 = 0x01,
	eIMGPROC_RGB555 = 0x02,
	eIMGPROC_RGB565 = 0x04,
	eIMGPROC_YUV422 = 0x08,
	eIMGPROC_YUV422P = 0x10,
	eIMGPROC_YUV420 = 0x11,
	eIMGPROC_YUV420P = 0x12,
	eIMGPROC_YUV420P_MB = 0x14
} E_IMGPROC_COLOR_FORMAT;


typedef enum {
	eIMGPROC_H_MIRROR = 0x01,
	eIMGPROC_V_MIRROR = 0x02,
	eIMGPROC_H_V_MIRROR = 0x04,
} E_IMGPROC_MIRROR_TYPE;

typedef enum {
	eIMGPROC_ROTATE90_RIGHT,
	eIMGPROC_ROTATE90_LEFT,
} E_IMGPROC_ROTATE;

typedef enum {
	eIMGPROC_OP_SCALE_NONE,
	eIMGPROC_OP_SCALE_MIRRORH,
	eIMGPROC_OP_SCALE_MIRRORV,
	eIMGPROC_OP_SCALE_MIRRORHV
} E_IMGPROC_MULTI_OP;

typedef struct {
	uint8_t *pu8SrcBuf;
	uint8_t *pu8DestBuf;
	uint32_t u32SrcBufPhyAdr;
	uint32_t u32DestBufPhyAdr;
	E_IMGPROC_COLOR_FORMAT eSrcFmt;
	E_IMGPROC_COLOR_FORMAT eDestFmt;
	uint32_t u32ImgWidth;
	uint32_t u32ImgHeight;
	uint32_t u32TransLen;
	uint32_t u32SrcImgStride;
	uint32_t u32DestImgStride;
} S_IMGPROC_CST_ATTR;

typedef struct {
	E_IMGPROC_COLOR_FORMAT eSrcFmt;
	E_IMGPROC_COLOR_FORMAT eDestFmt;
	E_IMGPROC_MIRROR_TYPE eMirrorType;
	uint32_t u32ImgWidth;
	uint32_t u32ImgHeight;
	uint32_t u32SrcImgStride;
	uint32_t u32DestImgStride;
} S_IMGPROC_MIRROR_PARAM;

typedef struct {
	S_IMGPROC_MIRROR_PARAM *psMirrorParam;
	uint8_t *pu8SrcBuf;
	uint8_t *pu8DestBuf;
	uint32_t u32SrcBufPhyAdr;
	uint32_t u32DestBufPhyAdr;
} S_IMGPROC_MIRROR_ATTR;

typedef struct {
	E_IMGPROC_COLOR_FORMAT eSrcFmt;
	E_IMGPROC_COLOR_FORMAT eDestFmt;
	uint32_t u32SrcImgWidth;
	uint32_t u32SrcImgHeight;
	uint32_t u32SrcImgStride;
	uint32_t u32CropWinWidth;
	uint32_t u32CropWinHeight;
	uint32_t u32CropWinStartX;
	uint32_t u32CropWinStartY;
	uint32_t u32ScaleImgWidth;
	uint32_t u32ScaleImgHeight;
	uint32_t u32ScaleImgStride;
	uint32_t u32ScaleInXFac;
	uint32_t u32ScaleOutXFac;
	uint32_t u32ScaleInYFac;
	uint32_t u32ScaleOutYFac;
} S_IMGPROC_SCALE_PARAM;

typedef struct {
	S_IMGPROC_SCALE_PARAM *psScaleUpParam;
	uint8_t *pu8SrcBuf;
	uint8_t *pu8DestBuf;
	uint32_t u32SrcBufPhyAdr;
	uint32_t u32DestBufPhyAdr;
} S_IMGPROC_SCALE_ATTR;

typedef struct {
	E_IMGPROC_ROTATE eRotate;
	E_IMGPROC_COLOR_FORMAT eSrcFmt;
	E_IMGPROC_COLOR_FORMAT eDestFmt;
	uint32_t u32SrcImgWidth;
	uint32_t u32SrcImgHeight;
	uint32_t u32DestImgWidth;
	uint32_t u32DestImgHeight;
	uint32_t u32SrcRightOffset;
	uint32_t u32SrcLeftOffset;
	uint32_t u32DestRightOffset;
	uint32_t u32DestLeftOffset;
} S_IMGPROC_ROTATE_PARAM;

typedef struct {
	S_IMGPROC_ROTATE_PARAM *psRotateParam;
	uint8_t *pu8SrcBuf;
	uint8_t *pu8DestBuf;
	uint32_t u32SrcBufPhyAdr;
	uint32_t u32DestBufPhyAdr;
} S_IMGPROC_ROTATE_ATTR;


typedef struct {
	E_IMGPROC_COLOR_FORMAT eSrcFmt;
	E_IMGPROC_COLOR_FORMAT eDestFmt;
	E_IMGPROC_MULTI_OP	eMultiOP;
	uint32_t u32SrcImgWidth;
	uint32_t u32SrcImgHeight;
	uint32_t u32SrcImgStrideW;
	uint32_t u32SrcImgStrideH;
	uint32_t u32CropWinWidth;
	uint32_t u32CropWinHeight;
	uint32_t u32CropWinStartX;
	uint32_t u32CropWinStartY;
	uint32_t u32DestImgWidth;
	uint32_t u32DestImgHeight;
	uint32_t u32DestImgStrideW;
} S_IMGPROC_MULTI_OP_PARAM;

typedef struct {
	S_IMGPROC_MULTI_OP_PARAM *psMultiOPParam;
	uint8_t *pu8SrcBuf;
	uint8_t *pu8DestBuf;
	uint32_t u32SrcBufPhyAdr;
	uint32_t u32DestBufPhyAdr;
} S_IMGPROC_MULTI_OP_ATTR;

typedef struct {
	uint8_t *pu8SrcBuf;
	uint8_t *pu8DestBuf;
	uint32_t u32SrcBufPhyAdr;
	uint32_t u32DestBufPhyAdr;
	E_IMGPROC_COLOR_FORMAT eSrcFmt;
	E_IMGPROC_COLOR_FORMAT eDestFmt;
	uint32_t u32ImgWidth;
	uint32_t u32ImgHeight;
	uint32_t u32SrcImgStride;
	uint32_t u32DestImgStride;
} S_IMGPROC_SMOOTH_ATTR;

typedef struct {
	uint8_t *pu8SrcBuf;
	uint8_t *pu8DestBuf;
	uint32_t u32SrcBufPhyAdr;
	uint32_t u32DestBufPhyAdr;
	E_IMGPROC_COLOR_FORMAT eSrcFmt;
	E_IMGPROC_COLOR_FORMAT eDestFmt;
	uint32_t u32ImgWidth;
	uint32_t u32ImgHeight;
	uint32_t u32SrcImgStride;
	uint32_t u32DestImgStride;
} S_IMGPROC_SHARP_ATTR;

ERRCODE
ImgProc_CST(
	E_IMGPROC_OP_MODE eOPmode,
	S_IMGPROC_CST_ATTR *psCSTAttr
);

ERRCODE
ImgProc_Mirror(
	E_IMGPROC_OP_MODE eOPmode,
	S_IMGPROC_MIRROR_ATTR *psMirrorAttr
);

ERRCODE
ImgProc_Scale(
	E_IMGPROC_OP_MODE eOPmode,
	S_IMGPROC_SCALE_ATTR *psScaleUpAttr
);

ERRCODE
ImgProc_Rotate(
	E_IMGPROC_OP_MODE eOPmode,
	S_IMGPROC_ROTATE_ATTR *psRotateAttr
);

ERRCODE
ImgProc_MultiOP(
	E_IMGPROC_OP_MODE eOPmode,
	S_IMGPROC_MULTI_OP_ATTR *psOPAttr
);

ERRCODE
ImgProc_Smooth(
	E_IMGPROC_OP_MODE eOPmode,
	S_IMGPROC_SMOOTH_ATTR *psSmoothAttr
);

ERRCODE
ImgProc_Sharp(
	E_IMGPROC_OP_MODE eOPmode,
	S_IMGPROC_SHARP_ATTR *psSharpAttr
);



#endif
