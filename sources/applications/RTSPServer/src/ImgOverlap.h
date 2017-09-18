/* ImgOverlap.h
 *
 *
 * Copyright (c)2013 Nuvoton technology corporation
 * http://www.nuvoton.com
 *
 * Image overlap function
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef __IMG_OVERLAP_H__
#define __IMG_OVERLAP_H__

#include "ImgProc.h"



enum {
	// error code
	eIMG_OVERLAP_ERROR_NONE = 0,
	eIMG_OVERLAP_ERROR_POS = -1,
	eIMG_OVERLAP_ERROR_SIZE = -2,
	eIMG_OVERLAP_ERROR_MB_BOUNDARY = -3,
};




int32_t
ImgOverlap_Overlap(
	E_IMGPROC_COLOR_FORMAT eBGColor,	//Background color format
	uint8_t *pu8BGImg,					//Background image address
	uint32_t u32BGImgWidth,				//Background image width
	uint32_t u32BGImgHeight,			//Background image height
	uint8_t *pu8OPImg,					//Overlap image address
	uint32_t u32OPImgWidth,				//Overlap image width
	uint32_t u32OPImgHeight,			//Overlap image height
	uint32_t u32PosX,					//Overlap image poisition x
	uint32_t u32PosY					//Overlap image poisition y
);

#endif

