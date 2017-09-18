/*
 * ImgOverlap.c
 * Copyright (C) nuvoTon
 *
 * TextOverlap.c is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * NMRecorder.c is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ImgOverlap.h"


#define ONE_MB_Y_WIDTH 		16
#define ONE_MB_Y_HEIGHT 	16
#define ONE_MB_Y_SIZE		(ONE_MB_Y_WIDTH * ONE_MB_Y_HEIGHT)


#define ONE_MB_UV_WIDTH 	8
#define ONE_MB_UV_HEIGHT 	16
#define ONE_MB_UV_SIZE		(ONE_MB_UV_WIDTH * ONE_MB_UV_HEIGHT)

static void
Overlap_YUV422(
	uint8_t *pu8BGImg,
	uint32_t u32BGImgWidth,
	uint32_t u32BGImgHeight,
	uint8_t *pu8OPImg,					//Overlap image address
	uint32_t u32OPImgWidth,				//Overlap image width
	uint32_t u32OPImgHeight,			//Overlap image height
	uint32_t u32PosX,
	uint32_t u32PosY
)
{
	uint8_t *pu8BGYAddr;
	uint8_t *pu8OverlapYAddr;
	uint32_t y;
	uint32_t u32CopyImgWidth;
	uint32_t u32CopyImgHeight;

	if ((u32PosX + u32OPImgWidth) > u32BGImgWidth) {
		u32CopyImgWidth = u32BGImgWidth - u32PosX;
	}
	else {
		u32CopyImgWidth = u32OPImgWidth;
	}

	if ((u32PosY + u32OPImgHeight) > u32BGImgHeight) {
		u32CopyImgHeight = u32BGImgHeight - u32PosY;
	}
	else {
		u32CopyImgHeight = u32OPImgHeight;
	}

	pu8BGYAddr = pu8BGImg + (((u32PosY * u32BGImgWidth) + u32PosX) * 2);
	pu8OverlapYAddr = pu8OPImg;

	for (y = 0; y < u32CopyImgHeight; y ++) {
		memcpy(pu8BGYAddr, pu8OverlapYAddr, u32CopyImgWidth * 2);

		pu8BGYAddr += (u32BGImgWidth * 2);
		pu8OverlapYAddr += (u32OPImgWidth * 2);
	}
}

static void
Overlap_YUV420(
	uint8_t *pu8BGImg,
	uint32_t u32BGImgWidth,
	uint32_t u32BGImgHeight,
	uint8_t *pu8OPImg,					//Overlap image address
	uint32_t u32OPImgWidth,				//Overlap image width
	uint32_t u32OPImgHeight,			//Overlap image height
	uint32_t u32PosX,
	uint32_t u32PosY
)
{
	uint8_t *pu8BGYAddr;

	uint8_t *pu8OverlapYAddr;
	uint32_t y;

	uint32_t u32CopyImgWidth;
	uint32_t u32CopyImgHeight;

	if ((u32PosX + u32OPImgWidth) > u32BGImgWidth) {
		u32CopyImgWidth = u32BGImgWidth - u32PosX;
	}
	else {
		u32CopyImgWidth = u32OPImgWidth;
	}

	if ((u32PosY + u32OPImgHeight) > u32BGImgHeight) {
		u32CopyImgHeight = u32BGImgHeight - u32PosY;
	}
	else {
		u32CopyImgHeight = u32OPImgHeight;
	}

	pu8BGYAddr = pu8BGImg + ((((u32PosY * u32BGImgWidth) + u32PosX) / 2) * 3);
	pu8OverlapYAddr = pu8OPImg;

	for (y = 0; y < u32CopyImgHeight; y ++) {
		memcpy(pu8BGYAddr, pu8OverlapYAddr, u32CopyImgWidth / 2 * 3);

		pu8BGYAddr += (u32BGImgWidth / 2 * 3);
		pu8OverlapYAddr += (u32OPImgWidth / 2 * 3);
	}

}

static void
Overlap_YUV422P(
	uint8_t *pu8BGImg,
	uint32_t u32BGImgWidth,
	uint32_t u32BGImgHeight,
	uint8_t *pu8OPImg,					//Overlap image address
	uint32_t u32OPImgWidth,				//Overlap image width
	uint32_t u32OPImgHeight,			//Overlap image height
	uint32_t u32PosX,
	uint32_t u32PosY
)
{
	uint8_t *pu8BGYAddr;
	uint8_t *pu8BGUAddr;
	uint8_t *pu8BGVAddr;

	uint8_t *pu8OverlapYAddr;
	uint8_t *pu8OverlapUAddr;
	uint8_t *pu8OverlapVAddr;

	uint32_t y;

	uint32_t u32CopyImgWidth;
	uint32_t u32CopyImgHeight;

	if ((u32PosX + u32OPImgWidth) > u32BGImgWidth) {
		u32CopyImgWidth = u32BGImgWidth - u32PosX;
	}
	else {
		u32CopyImgWidth = u32OPImgWidth;
	}

	if ((u32PosY + u32OPImgHeight) > u32BGImgHeight) {
		u32CopyImgHeight = u32BGImgHeight - u32PosY;
	}
	else {
		u32CopyImgHeight = u32OPImgHeight;
	}

	pu8BGYAddr = pu8BGImg + ((u32PosY * u32BGImgWidth) + u32PosX);
	pu8BGUAddr = pu8BGImg + u32BGImgWidth * u32BGImgHeight;
	pu8BGUAddr += (((u32PosY * u32BGImgWidth) + u32PosX) / 2);
	pu8BGVAddr = pu8BGUAddr + ((u32BGImgWidth * u32BGImgHeight) / 2);

	pu8OverlapYAddr = pu8OPImg;
	pu8OverlapUAddr = pu8OverlapYAddr + (u32OPImgWidth * u32OPImgHeight);
	pu8OverlapVAddr = pu8OverlapUAddr + ((u32OPImgWidth * u32OPImgHeight) / 2);

	for (y = 0; y < u32CopyImgHeight; y ++) {

		memcpy(pu8BGYAddr, pu8OverlapYAddr, u32CopyImgWidth);
		memcpy(pu8BGUAddr, pu8OverlapUAddr, u32CopyImgWidth / 2);
		memcpy(pu8BGVAddr, pu8OverlapVAddr, u32CopyImgWidth / 2);

		pu8BGYAddr += u32BGImgWidth;
		pu8BGUAddr += u32BGImgWidth / 2;
		pu8BGVAddr += u32BGImgWidth / 2;

		pu8OverlapYAddr += u32OPImgWidth;
		pu8OverlapUAddr += u32OPImgWidth / 2;
		pu8OverlapVAddr += u32OPImgWidth / 2;
	}
}

static void
Overlap_YUV420P(
	uint8_t *pu8BGImg,
	uint32_t u32BGImgWidth,
	uint32_t u32BGImgHeight,
	uint8_t *pu8OPImg,					//Overlap image address
	uint32_t u32OPImgWidth,				//Overlap image width
	uint32_t u32OPImgHeight,			//Overlap image height
	uint32_t u32PosX,
	uint32_t u32PosY
)
{
	uint8_t *pu8BGYAddr;
	uint8_t *pu8BGUAddr;
	uint8_t *pu8BGVAddr;
	uint8_t *pu8BGYAddr_next;

	uint8_t *pu8OverlapYAddr;
	uint8_t *pu8OverlapUAddr;
	uint8_t *pu8OverlapVAddr;
	uint8_t *pu8OverlapYAddr_next;

	uint32_t y;

	uint32_t u32CopyImgWidth;
	uint32_t u32CopyImgHeight;

	if ((u32PosX + u32OPImgWidth) > u32BGImgWidth) {
		u32CopyImgWidth = u32BGImgWidth - u32PosX;
	}
	else {
		u32CopyImgWidth = u32OPImgWidth;
	}

	if ((u32PosY + u32OPImgHeight) > u32BGImgHeight) {
		u32CopyImgHeight = u32BGImgHeight - u32PosY;
	}
	else {
		u32CopyImgHeight = u32OPImgHeight;
	}

	pu8BGYAddr = pu8BGImg + ((u32PosY * u32BGImgWidth) + u32PosX);
	pu8BGUAddr = pu8BGImg + u32BGImgWidth * u32BGImgHeight;
	pu8BGUAddr += ((u32PosY / 2) * (u32BGImgWidth / 2)) + (u32PosX / 2);
	pu8BGVAddr = pu8BGUAddr + ((u32BGImgWidth * u32BGImgHeight) / 4);

	pu8OverlapYAddr = pu8OPImg;
	pu8OverlapUAddr = pu8OverlapYAddr + (u32OPImgWidth * u32OPImgHeight);
	pu8OverlapVAddr = pu8OverlapUAddr + ((u32OPImgWidth * u32OPImgHeight) / 4);

	for (y = 0; y < u32CopyImgHeight; y += 2) {

		pu8BGYAddr_next = pu8BGYAddr + u32BGImgWidth;
		pu8OverlapYAddr_next = pu8OverlapYAddr + u32OPImgWidth;

		memcpy(pu8BGYAddr, pu8OverlapYAddr, u32CopyImgWidth);
		memcpy(pu8BGYAddr_next, pu8OverlapYAddr_next, u32CopyImgWidth);
		memcpy(pu8BGUAddr, pu8OverlapUAddr, u32CopyImgWidth / 2);
		memcpy(pu8BGVAddr, pu8OverlapVAddr, u32CopyImgWidth / 2);

		pu8BGYAddr += u32BGImgWidth * 2;
		pu8BGUAddr += u32BGImgWidth / 2;
		pu8BGVAddr += u32BGImgWidth / 2;

		pu8OverlapYAddr += u32OPImgWidth * 2;
		pu8OverlapUAddr += u32OPImgWidth / 2;
		pu8OverlapVAddr += u32OPImgWidth / 2;
	}

}

static void
Overlap_YUV420MB(
	uint8_t *pu8BGImg,
	uint32_t u32BGImgWidth,
	uint32_t u32BGImgHeight,
	uint8_t *pu8OPImg,					//Overlap image address
	uint32_t u32OPImgWidth,				//Overlap image width
	uint32_t u32OPImgHeight,			//Overlap image height
	uint32_t u32PosX,
	uint32_t u32PosY
)
{
	uint8_t *pu8BGYAddr;
	uint8_t *pu8BGYPlusAddr;
	uint8_t *pu8BGUVAddr;
	uint8_t *pu8BGUAddr;
	uint8_t *pu8BGVAddr;

	uint8_t *pu8OverlapYAddr;
	uint8_t *pu8OverlapYPlusAddr;
	uint8_t *pu8OverlapUAddr;
	uint8_t *pu8OverlapVAddr;

	uint32_t u32MBLine = 0;
	uint32_t u32MBCol = 0;
	uint32_t u32LineOffInOneMB = 0;
	uint32_t u32ColOffInOneMB = 0;
	uint32_t u32MBYPos = 0;
	uint32_t u32MBUVPos = 0;

	uint32_t u32WidthInMB;


	uint32_t u32OverlapMB_Width = u32OPImgWidth / ONE_MB_Y_WIDTH;
	uint32_t u32OverlapMB_Height = u32OPImgHeight / ONE_MB_Y_HEIGHT;

	uint32_t u32MB_y;

	//TODO copy image width and height
	uint32_t u32CopyImgWidth;
	uint32_t u32CopyImgHeight;

	if ((u32PosX + u32OPImgWidth) > u32BGImgWidth) {
		u32CopyImgWidth = u32BGImgWidth - u32PosX;
	}
	else {
		u32CopyImgWidth = u32OPImgWidth;
	}

	if ((u32PosY + u32OPImgHeight) > u32BGImgHeight) {
		u32CopyImgHeight = u32BGImgHeight - u32PosY;
	}
	else {
		u32CopyImgHeight = u32OPImgHeight;
	}

	uint32_t u32CopyImgMB_Width = u32CopyImgWidth / ONE_MB_Y_WIDTH;
	uint32_t u32CopyImgMB_Height = u32CopyImgHeight / ONE_MB_Y_HEIGHT;

	pu8OverlapYAddr = pu8OPImg;
	pu8OverlapYPlusAddr = pu8OverlapYAddr + ONE_MB_Y_WIDTH;
	pu8OverlapUAddr = pu8OverlapYAddr + u32OPImgWidth * u32OPImgHeight;
	pu8OverlapVAddr = pu8OverlapUAddr + ONE_MB_UV_WIDTH;

	for (u32MB_y = 0; u32MB_y < u32CopyImgMB_Height; u32MB_y ++) {

		// Get BG MB Y address
		u32WidthInMB = (u32BGImgWidth + ONE_MB_Y_WIDTH - 1) / ONE_MB_Y_WIDTH;

		u32MBLine = u32PosY / ONE_MB_Y_HEIGHT;
		u32LineOffInOneMB = u32PosY & (ONE_MB_Y_HEIGHT - 1);

		u32MBCol = u32PosX / ONE_MB_Y_WIDTH;
		u32ColOffInOneMB = u32PosX & (ONE_MB_Y_WIDTH - 1);

		u32MBYPos = (u32MBLine * u32WidthInMB + u32MBCol) * ONE_MB_Y_SIZE;
		u32MBYPos = u32MBYPos + (u32LineOffInOneMB * ONE_MB_Y_WIDTH + u32ColOffInOneMB);

		pu8BGYAddr = pu8BGImg + u32MBYPos;
		pu8BGYPlusAddr = pu8BGYAddr + ONE_MB_Y_WIDTH;

		// Get BG MB UV address
		u32WidthInMB = ((u32BGImgWidth / 2) + ONE_MB_UV_WIDTH - 1) / ONE_MB_UV_WIDTH;
		pu8BGUVAddr = pu8BGImg + (u32BGImgWidth * u32BGImgHeight);

		u32MBLine = u32PosY / ONE_MB_UV_HEIGHT;
		u32LineOffInOneMB = u32PosY & (ONE_MB_UV_HEIGHT - 1);

		u32MBCol = u32PosX / 2 / ONE_MB_UV_WIDTH;
		u32ColOffInOneMB = (u32PosX / 2) & (ONE_MB_UV_WIDTH - 1);

		u32MBUVPos = (u32MBLine * u32WidthInMB + u32MBCol) * ONE_MB_UV_SIZE;
		u32MBUVPos = u32MBUVPos + (u32LineOffInOneMB * ONE_MB_UV_WIDTH + u32ColOffInOneMB);

		pu8BGUAddr = pu8BGUVAddr + u32MBUVPos;
		pu8BGVAddr = pu8BGUAddr + ONE_MB_UV_WIDTH;

		memcpy(pu8BGYAddr, pu8OverlapYAddr, ONE_MB_Y_SIZE * u32CopyImgMB_Width);
		memcpy(pu8BGUAddr, pu8OverlapUAddr, ONE_MB_UV_SIZE * u32CopyImgMB_Width);

		pu8BGYAddr += ONE_MB_Y_SIZE * u32OverlapMB_Width;
		pu8BGUAddr += ONE_MB_UV_SIZE * u32OverlapMB_Width;

		pu8OverlapYAddr += ONE_MB_Y_SIZE * u32OverlapMB_Width;
		pu8OverlapUAddr += ONE_MB_UV_SIZE * u32OverlapMB_Width;

		u32PosY += ONE_MB_Y_HEIGHT;
	}

}


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
)
{

	if (u32PosX & 0x01)
		u32PosX --;

	if ((eBGColor == eIMGPROC_YUV420) || (eBGColor == eIMGPROC_YUV420P))
		if (u32PosY & 0x01)
			u32PosY --;

	//	if((u32PosX + u32OPImgWidth) > u32BGImgWidth)
	//		return eIMG_OVERLAP_ERROR_SIZE;

	//	if((u32PosY + u32OPImgHeight) > u32BGImgHeight)
	//		return eIMG_OVERLAP_ERROR_SIZE;

	if (eBGColor == eIMGPROC_YUV420P_MB) {
		if (u32OPImgWidth & (ONE_MB_Y_WIDTH - 1)) {
			printf("[ImgOverlap]: Imgge width must align to marco block boundary\n");
			return eIMG_OVERLAP_ERROR_MB_BOUNDARY;
		}

		if (u32OPImgHeight & (ONE_MB_Y_HEIGHT - 1)) {
			printf("[ImgOverlap]: Imgge height must align to marco block boundary\n");
			return eIMG_OVERLAP_ERROR_MB_BOUNDARY;
		}

		if (u32PosX & (ONE_MB_Y_WIDTH - 1)) {
			printf("[ImgOverlap]: u32PosX must align to marco block boundary\n");
			return eIMG_OVERLAP_ERROR_MB_BOUNDARY;
		}

		if (u32PosY & (ONE_MB_Y_HEIGHT - 1)) {
			printf("[ImgOverlap]: u32PosY must align to marco block boundary\n");
			return eIMG_OVERLAP_ERROR_MB_BOUNDARY;
		}
	}

	//Overlap image
	if (eBGColor == eIMGPROC_YUV420P_MB)
		Overlap_YUV420MB(pu8BGImg, u32BGImgWidth, u32BGImgHeight, pu8OPImg, u32OPImgWidth, u32OPImgHeight, u32PosX, u32PosY);
	else if (eBGColor == eIMGPROC_YUV422)
		Overlap_YUV422(pu8BGImg, u32BGImgWidth, u32BGImgHeight, pu8OPImg, u32OPImgWidth, u32OPImgHeight, u32PosX, u32PosY);
	else if (eBGColor == eIMGPROC_YUV422P)
		Overlap_YUV422P(pu8BGImg, u32BGImgWidth, u32BGImgHeight, pu8OPImg, u32OPImgWidth, u32OPImgHeight, u32PosX, u32PosY);
	else if (eBGColor == eIMGPROC_YUV420)
		Overlap_YUV420(pu8BGImg, u32BGImgWidth, u32BGImgHeight, pu8OPImg, u32OPImgWidth, u32OPImgHeight, u32PosX, u32PosY);
	else if (eBGColor == eIMGPROC_YUV420P)
		Overlap_YUV420P(pu8BGImg, u32BGImgWidth, u32BGImgHeight, pu8OPImg, u32OPImgWidth, u32OPImgHeight, u32PosX, u32PosY);

	return eIMG_OVERLAP_ERROR_NONE;
}
