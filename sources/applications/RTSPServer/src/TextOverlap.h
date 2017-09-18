/* FileMgr.h
 *
 *
 * Copyright (c)2013 Nuvoton technology corporation
 * http://www.nuvoton.com
 *
 * Text overlap image function
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef __TEXT_OVERLAP_H__
#define __TEXT_OVERLAP_H__

#include "ImgProc.h"

typedef uint32_t	H_TEXT_OVERLAP;


enum {
	eTEXT_OVERLAP_INVALID_HANDLE = 0,


	// error code
	eTEXT_OVERLAP_ERROR_NONE = 0,
	eTEXT_OVERLAP_ERROR_INVALID_HANDLE = -1,
	eTEXT_OVERLAP_ERROR_SIZE = -2,
	eTEXT_OVERLAP_ERROR_MB_BOUNDARY = -3,
};

H_TEXT_OVERLAP
TextOverlap_Init(
	uint32_t u32FontWidth,				//Font width
	uint32_t u32FontHeight,				//Font height
	E_IMGPROC_COLOR_FORMAT eBGColor,	//Background color format
	uint32_t u32MaxTexts,				//Maxium texts support
	uint32_t u32ColorKeyY,				//Color key Y componment
	uint32_t u32ColorKeyU,				//Color key U componment
	uint32_t u32ColorKeyV				//Color key V componment
);

int32_t
TextOverlap_Overlap(
	H_TEXT_OVERLAP hTextOverlap,
	uint8_t *apu8TextAddr[],
	uint32_t u32NumTexts,
	uint8_t *pu8BGImg,
	E_IMGPROC_COLOR_FORMAT eBGColor,
	uint32_t u32BGImgWidth,
	uint32_t u32BGImgHeight,
	uint32_t u32PosX,
	uint32_t u32PosY
);

void
TextOverlap_UnInit(
	H_TEXT_OVERLAP *phTextOverlap
);


#endif

