/*
 * TextOverlap.c
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

#include "TextOverlap.h"

#define ONE_MB_Y_WIDTH 		16
#define ONE_MB_Y_HEIGHT 	16
#define ONE_MB_Y_SIZE		(ONE_MB_Y_WIDTH * ONE_MB_Y_HEIGHT)


#define ONE_MB_UV_WIDTH 	8
#define ONE_MB_UV_HEIGHT 	16
#define ONE_MB_UV_SIZE		(ONE_MB_UV_WIDTH * ONE_MB_UV_HEIGHT)


typedef struct S_TEXT_OVERLAP {
	uint32_t u32FontWidth;				//Font width
	uint32_t u32FontHeight; 			//Font height
	uint32_t u32MaxTexts;				//Maxinum texts supported
	uint8_t *pu8TextImg;				//Pack text to YUV422 image
	uint8_t *pu8TextBGImg;				//Convert YUV422 to BG color image
	uint32_t u32TextImgWidth;			//Text image width
	uint32_t u32TextImgHeight;			//Text image height
	uint32_t u32ColorKeyY;				//Color key Y componment
	uint32_t u32ColorKeyU;				//Color key U componment
	uint32_t u32ColorKeyV;				//Color key V componment
} S_TEXT_OVERLAP;


static void
PackTextImg(
	S_TEXT_OVERLAP *psTextOverlap,
	uint8_t *apu8TextAddr[],
	uint32_t u32NumTexts
)
{
	int32_t i, x, y;
	uint32_t u32FontWidth = psTextOverlap->u32FontWidth;
	uint32_t u32FontHeight = psTextOverlap->u32FontHeight;
	uint32_t u32TextImgWidth = u32FontWidth * u32NumTexts;
	uint32_t u32TextImgStride = u32TextImgWidth;
	uint8_t *pu8TextAddr;
	uint8_t *pu8DrawAddr;
	uint8_t *pu8TextImg = psTextOverlap->pu8TextImg;

	x = 0;

	for (i = 0; i < u32NumTexts; i ++) {
		pu8TextAddr = apu8TextAddr[i];

		for (y = 0; y < u32FontHeight; y++) {
			pu8DrawAddr = pu8TextImg + (((y * u32TextImgStride) + x) * 2);
			memcpy(pu8DrawAddr, pu8TextAddr, u32FontWidth * 2);
			pu8TextAddr += (u32FontWidth * 2);
		}

		x += u32FontWidth;
	}

	psTextOverlap->u32TextImgWidth = u32TextImgWidth;
	psTextOverlap->u32TextImgHeight = u32FontHeight;
}


static void 
YUV422ToYUV420Planar_SW(
	uint8_t *pu8SrcImg,
	uint8_t *pu8DestImg,
	uint16_t u16SrcImgWidth,
	uint16_t u16SrcImgHeight
)
{
	uint16_t x,y;
	uint16_t *pu16SrcImg = (uint16_t *)pu8SrcImg;
	uint8_t *pu8DestYComp = pu8DestImg;
	uint8_t *pu8DestUComp = pu8DestYComp + (u16SrcImgWidth*u16SrcImgHeight);
	uint8_t *pu8DestVComp = pu8DestUComp + ((u16SrcImgWidth*u16SrcImgHeight)/4);

	uint8_t u8Y0;
	uint8_t u8Y1;
	uint8_t u8U0;
	uint8_t u8V0;
	uint8_t u8Y2;
	uint8_t u8Y3;
	uint8_t u8U1;
	uint8_t u8V1;

	uint16_t u16Temp;
	uint32_t u32SrcY0Addr;
	uint32_t u32SrcY2Addr;
	uint32_t u32DestY0Addr;
	uint32_t u32DestY2Addr;

	for( y = 0; y < u16SrcImgHeight ; y += 2){
		u32SrcY0Addr = y * u16SrcImgWidth;
		u32SrcY2Addr = (y + 1) * u16SrcImgWidth;
		u32DestY0Addr = y * u16SrcImgWidth; 
		u32DestY2Addr = (y+1) * u16SrcImgWidth; 
		for( x = 0; x < u16SrcImgWidth ; x += 2){
			u16Temp = pu16SrcImg[u32SrcY0Addr + x];
			u8Y0 = (uint8_t)(u16Temp & 0x00FF);
			u8U0 = (uint8_t)((u16Temp & 0xFF00) >> 8);
			u16Temp = pu16SrcImg[u32SrcY0Addr + x + 1];
			u8Y1 = (uint8_t)(u16Temp & 0x00FF);
			u8V0 = (uint8_t)((u16Temp & 0xFF00) >> 8);
			u16Temp = pu16SrcImg[u32SrcY2Addr + x];
			u8Y2 = (uint8_t)(u16Temp & 0x00FF);
			u8U1 = (uint8_t)((u16Temp & 0xFF00) >> 8);
			u16Temp = pu16SrcImg[u32SrcY2Addr + x + 1];
			u8Y3 = (uint8_t)(u16Temp & 0x00FF);
			u8V1 = (uint8_t)((u16Temp & 0xFF00) >> 8);

			pu8DestYComp[u32DestY0Addr + x] = u8Y0;
			pu8DestYComp[u32DestY0Addr + x + 1] = u8Y1;
			pu8DestYComp[u32DestY2Addr + x] = u8Y2;
			pu8DestYComp[u32DestY2Addr + x + 1] = u8Y3;
			*pu8DestUComp = (u8U0 + u8U1) >> 1;
			*pu8DestVComp = (u8V0 + u8V1) >> 1;
			pu8DestUComp++;
			pu8DestVComp++;
		}
	}
}


ERRCODE ImgProc_CST(
	E_IMGPROC_OP_MODE eOPmode,
	S_IMGPROC_CST_ATTR *psCSTAttr
)
{
		if((psCSTAttr->pu8SrcBuf == NULL) || (psCSTAttr->pu8DestBuf == NULL))
			return ERR_IMGPROC_BAD_ATTR;
#if 1
		if((psCSTAttr->eSrcFmt == eIMGPROC_YUV422) && (psCSTAttr->eDestFmt == eIMGPROC_YUV420P)){
			//YUV422 to YUV420P
			YUV422ToYUV420Planar_SW(psCSTAttr->pu8SrcBuf, psCSTAttr->pu8DestBuf, psCSTAttr->u32ImgWidth, psCSTAttr->u32ImgHeight);
		}
#else
		if((psCSTAttr->eSrcFmt == eIMGPROC_YUV422) && (psCSTAttr->eDestFmt == eIMGPROC_YUV422P)){
			//YUV422 to YUV422P
			YUV422PacketToPlanar_SW(psCSTAttr->pu8SrcBuf, psCSTAttr->pu8DestBuf, psCSTAttr->u32ImgWidth, psCSTAttr->u32ImgHeight);
		}
		else if((psCSTAttr->eSrcFmt == eIMGPROC_YUV422P) && (psCSTAttr->eDestFmt == eIMGPROC_YUV422)){
			//YUV422P to YUV422
			YUV422PlanarToPacket_SW(psCSTAttr->pu8SrcBuf, psCSTAttr->pu8DestBuf, psCSTAttr->u32ImgWidth, psCSTAttr->u32ImgHeight);
		}
		else if((psCSTAttr->eSrcFmt == eIMGPROC_YUV422) && (psCSTAttr->eDestFmt == eIMGPROC_YUV420)){
			//YUV422 to YUV420
			YUV422ToYUV420_SW(psCSTAttr->pu8SrcBuf, psCSTAttr->pu8DestBuf, psCSTAttr->u32ImgWidth, psCSTAttr->u32ImgHeight);
		}
		else if((psCSTAttr->eSrcFmt == eIMGPROC_YUV420) && (psCSTAttr->eDestFmt == eIMGPROC_YUV422)){
			//YUV420 to YUV422
			YUV420ToYUV422_SW(psCSTAttr->pu8SrcBuf, psCSTAttr->pu8DestBuf, psCSTAttr->u32ImgWidth, psCSTAttr->u32ImgHeight);
		}
		else if((psCSTAttr->eSrcFmt == eIMGPROC_YUV420P) && (psCSTAttr->eDestFmt == eIMGPROC_YUV422)){
			//YUV420P to YUV422
			YUV420PlanarToYUV422_SW(psCSTAttr->pu8SrcBuf, psCSTAttr->pu8DestBuf, psCSTAttr->u32ImgWidth, psCSTAttr->u32ImgHeight);
		}
		else if((psCSTAttr->eSrcFmt == eIMGPROC_YUV422) && (psCSTAttr->eDestFmt == eIMGPROC_YUV420P)){
			//YUV422 to YUV420P
			YUV422ToYUV420Planar_SW(psCSTAttr->pu8SrcBuf, psCSTAttr->pu8DestBuf, psCSTAttr->u32ImgWidth, psCSTAttr->u32ImgHeight);
		}
		else if((psCSTAttr->eSrcFmt == eIMGPROC_YUV420P_MB) && (psCSTAttr->eDestFmt == eIMGPROC_YUV422)){
			//YUV420_MB to YUV422
			YUV420MBToYUV422_SW(psCSTAttr->pu8SrcBuf, psCSTAttr->pu8DestBuf, psCSTAttr->u32ImgWidth, psCSTAttr->u32ImgHeight);
		}
		else if((psCSTAttr->eSrcFmt == eIMGPROC_YUV422) && (psCSTAttr->eDestFmt == eIMGPROC_YUV420P_MB)){
			//YUV422 to YUV420_MB
			YUV422ToYUV420MB_SW(psCSTAttr->pu8SrcBuf, psCSTAttr->pu8DestBuf, psCSTAttr->u32ImgWidth, psCSTAttr->u32ImgHeight);
		}
		else {
			return ERR_IMGPROC_NOT_SUPP;
		}
#endif	
	return ERR_IMGPROC_SUCCESS;
} 

static void
ColorTrans(
	S_TEXT_OVERLAP *psTextOverlap,
	E_IMGPROC_COLOR_FORMAT eBGColor	//Background color format
)
{
	S_IMGPROC_CST_ATTR sImgProcAttr;

	if (eBGColor == eIMGPROC_YUV422) {
		memcpy(psTextOverlap->pu8TextBGImg,
			   psTextOverlap->pu8TextImg,
			   psTextOverlap->u32TextImgWidth * psTextOverlap->u32TextImgHeight * 2);
		return;
	}

	sImgProcAttr.pu8SrcBuf = psTextOverlap->pu8TextImg;
	sImgProcAttr.pu8DestBuf = psTextOverlap->pu8TextBGImg;
	sImgProcAttr.u32SrcBufPhyAdr = 0;
	sImgProcAttr.u32DestBufPhyAdr = 0;
	sImgProcAttr.eSrcFmt = eIMGPROC_YUV422;
	sImgProcAttr.eDestFmt = eBGColor;
	sImgProcAttr.u32ImgWidth = psTextOverlap->u32TextImgWidth;
	sImgProcAttr.u32ImgHeight = psTextOverlap->u32TextImgHeight;
	sImgProcAttr.u32TransLen = psTextOverlap->u32TextImgWidth * psTextOverlap->u32TextImgHeight * 2;
	sImgProcAttr.u32SrcImgStride = psTextOverlap->u32TextImgWidth;
	sImgProcAttr.u32DestImgStride = psTextOverlap->u32TextImgWidth;

	ImgProc_CST(eIMGPROC_OP_SW, &sImgProcAttr);
}

static void
Overlap_YUV422(
	S_TEXT_OVERLAP *psTextOverlap,
	uint8_t *pu8BGImg,
	uint32_t u32BGImgWidth,
	uint32_t u32BGImgHeight,
	uint32_t u32PosX,
	uint32_t u32PosY
)
{
	uint8_t *pu8BGYAddr;
	uint8_t *pu8BGTempYAddr;

	uint8_t *pu8OverlapYAddr;
	uint8_t *pu8OverlapTempYAddr;

	uint32_t u32OverlapWidth = psTextOverlap->u32TextImgWidth;
	uint32_t u32OverlapHeight = psTextOverlap->u32TextImgHeight;
	uint8_t u8ColorKeyY = psTextOverlap->u32ColorKeyY;
#if 0
	uint8_t u8ColorKeyU = psTextOverlap->u32ColorKeyU;
	uint8_t u8ColorKeyV = psTextOverlap->u32ColorKeyV;
#endif

	uint32_t x, y;

	uint8_t u8Y;
	uint8_t u8Y_Next;
	uint8_t u8U;
	uint8_t u8V;

	pu8BGTempYAddr = pu8BGImg + (((u32PosY * u32BGImgWidth) + u32PosX) * 2);
	pu8OverlapTempYAddr = psTextOverlap->pu8TextBGImg;

	for (y = 0; y < u32OverlapHeight; y ++) {
		pu8BGYAddr = pu8BGTempYAddr;
		pu8OverlapYAddr = pu8OverlapTempYAddr;

		for (x = 0; x < u32OverlapWidth; x += 2) {
			u8Y = pu8OverlapYAddr[0];
			u8U = pu8OverlapYAddr[1];
			u8Y_Next = pu8OverlapYAddr[2];
			u8V = pu8OverlapYAddr[3];

#if 0

			if ((u8Y != u8ColorKeyY) ||
					(u8U != u8ColorKeyU) ||
					(u8V != u8ColorKeyV)) {
#else

			if ((u8Y != u8ColorKeyY)) {
#endif
				pu8BGYAddr[0] = u8Y;
				pu8BGYAddr[1] = u8U;
				pu8BGYAddr[3] = u8V;
			}

#if 0

			if ((u8Y_Next != u8ColorKeyY) ||
					(u8U != u8ColorKeyU) ||
					(u8V != u8ColorKeyV)) {
#else

			if ((u8Y_Next != u8ColorKeyY)) {
#endif
				pu8BGYAddr[1] = u8U;
				pu8BGYAddr[2] = u8Y_Next;
				pu8BGYAddr[3] = u8V;
			}

			pu8BGYAddr += 4;
			pu8OverlapYAddr += 4;
		}

		pu8BGTempYAddr += (u32BGImgWidth * 2);
		pu8OverlapTempYAddr += (u32OverlapWidth * 2);
	}
}


static void
Overlap_YUV420(
	S_TEXT_OVERLAP *psTextOverlap,
	uint8_t *pu8BGImg,
	uint32_t u32BGImgWidth,
	uint32_t u32BGImgHeight,
	uint32_t u32PosX,
	uint32_t u32PosY
)
{
	uint8_t *pu8BGYAddr;
	uint8_t *pu8BGYAddr_Next;
	uint8_t *pu8BGTempYAddr;

	uint8_t *pu8OverlapYAddr;
	uint8_t *pu8OverlapYAddr_Next;
	uint8_t *pu8OverlapTempYAddr;

	uint32_t u32OverlapWidth = psTextOverlap->u32TextImgWidth;
	uint32_t u32OverlapHeight = psTextOverlap->u32TextImgHeight;
	uint8_t u8ColorKeyY = psTextOverlap->u32ColorKeyY;
#if 0
	uint8_t u8ColorKeyU = psTextOverlap->u32ColorKeyU;
	uint8_t u8ColorKeyV = psTextOverlap->u32ColorKeyV;
#endif

	uint32_t x, y;

	uint8_t u8Y0;
	uint8_t u8Y0_Next;
	uint8_t u8Y1;
	uint8_t u8Y1_Next;
	uint8_t u8U;
	uint8_t u8V;

	pu8BGTempYAddr = pu8BGImg + ((((u32PosY * u32BGImgWidth) + u32PosX) / 2) * 3);
	pu8OverlapTempYAddr = psTextOverlap->pu8TextBGImg;

	for (y = 0; y < u32OverlapHeight; y += 2) {
		pu8BGYAddr = pu8BGTempYAddr;
		pu8OverlapYAddr = pu8OverlapTempYAddr;
		pu8BGYAddr_Next = pu8BGYAddr + ((u32BGImgWidth / 2) * 3);
		pu8OverlapYAddr_Next = pu8OverlapYAddr + ((u32OverlapWidth / 2) * 3);

		for (x = 0; x < u32OverlapWidth; x += 2) {
			u8Y0 = pu8OverlapYAddr[0];
			u8Y1 = pu8OverlapYAddr[1];
			u8U = pu8OverlapYAddr[2];
			u8Y0_Next = pu8OverlapYAddr_Next[0];
			u8Y1_Next = pu8OverlapYAddr_Next[1];
			u8V = pu8OverlapYAddr_Next[2];

			if ((u8Y0 != u8ColorKeyY)) {
				pu8BGYAddr[0] = u8Y0;
				pu8BGYAddr[2] = u8U;
				pu8BGYAddr_Next[2] = u8V;
			}

			if ((u8Y1 != u8ColorKeyY)) {
				pu8BGYAddr[1] = u8Y1;
				pu8BGYAddr[2] = u8U;
				pu8BGYAddr_Next[2] = u8V;
			}

			if ((u8Y0_Next != u8ColorKeyY)) {
				pu8BGYAddr_Next[0] = u8Y0_Next;
				pu8BGYAddr[2] = u8U;
				pu8BGYAddr_Next[2] = u8V;
			}

			if ((u8Y1_Next != u8ColorKeyY)) {
				pu8BGYAddr_Next[1] = u8Y1_Next;
				pu8BGYAddr[2] = u8U;
				pu8BGYAddr_Next[2] = u8V;
			}

			pu8BGYAddr += 3;
			pu8OverlapYAddr += 3;

			pu8BGYAddr_Next += 3;
			pu8OverlapYAddr_Next += 3;

		}

		pu8BGTempYAddr += (u32BGImgWidth * 3);
		pu8OverlapTempYAddr += (u32OverlapWidth * 3);
	}
}


static void
Overlap_YUV422P(
	S_TEXT_OVERLAP *psTextOverlap,
	uint8_t *pu8BGImg,
	uint32_t u32BGImgWidth,
	uint32_t u32BGImgHeight,
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

	uint8_t *pu8BGTempYAddr;
	uint8_t *pu8BGTempUAddr;
	uint8_t *pu8BGTempVAddr;

	uint8_t *pu8OverlapTempYAddr;
	uint8_t *pu8OverlapTempUAddr;
	uint8_t *pu8OverlapTempVAddr;

	uint32_t u32OverlapWidth = psTextOverlap->u32TextImgWidth;
	uint32_t u32OverlapHeight = psTextOverlap->u32TextImgHeight;
	uint8_t u8ColorKeyY = psTextOverlap->u32ColorKeyY;
#if 0
	uint8_t u8ColorKeyU = psTextOverlap->u32ColorKeyU;
	uint8_t u8ColorKeyV = psTextOverlap->u32ColorKeyV;
#endif

	uint32_t x, y;

	uint8_t u8Y;
	uint8_t u8Y_Next;
	uint8_t u8U;
	uint8_t u8V;

	pu8BGTempYAddr = pu8BGImg + ((u32PosY * u32BGImgWidth) + u32PosX);
	pu8BGTempUAddr = pu8BGImg + u32BGImgWidth * u32BGImgHeight;
	pu8BGTempUAddr += (((u32PosY * u32BGImgWidth) + u32PosX) / 2);
	pu8BGTempVAddr = pu8BGTempUAddr + ((u32BGImgWidth * u32BGImgHeight) / 2);

	pu8OverlapTempYAddr = psTextOverlap->pu8TextBGImg;
	pu8OverlapTempUAddr = pu8OverlapTempYAddr + (u32OverlapWidth * u32OverlapHeight);
	pu8OverlapTempVAddr = pu8OverlapTempUAddr + ((u32OverlapWidth * u32OverlapHeight) / 2);

	for (y = 0; y < u32OverlapHeight; y ++) {
		pu8BGYAddr = pu8BGTempYAddr;
		pu8BGUAddr = pu8BGTempUAddr;
		pu8BGVAddr = pu8BGTempVAddr;

		pu8OverlapYAddr = pu8OverlapTempYAddr;
		pu8OverlapUAddr = pu8OverlapTempUAddr;
		pu8OverlapVAddr = pu8OverlapTempVAddr;

		for (x = 0; x < u32OverlapWidth; x += 2) {
			u8Y = pu8OverlapYAddr[0];
			u8U = pu8OverlapUAddr[0];
			u8Y_Next = pu8OverlapYAddr[1];
			u8V = pu8OverlapVAddr[0];

#if 0

			if ((u8Y != u8ColorKeyY) ||
					(u8U != u8ColorKeyU) ||
					(u8V != u8ColorKeyV)) {
#else

			if ((u8Y != u8ColorKeyY)) {
#endif
				pu8BGYAddr[0] = u8Y;
				pu8BGUAddr[0] = u8U;
				pu8BGVAddr[0] = u8V;
			}

#if 0

			if ((u8Y_Next != u8ColorKeyY) ||
					(u8U != u8ColorKeyU) ||
					(u8V != u8ColorKeyV)) {
#else

			if ((u8Y_Next != u8ColorKeyY)) {
#endif
				pu8BGYAddr[1] = u8Y_Next;
				pu8BGUAddr[0] = u8U;
				pu8BGVAddr[0] = u8V;
			}

			pu8BGYAddr += 2;
			pu8BGUAddr ++;
			pu8BGVAddr ++;

			pu8OverlapYAddr += 2;
			pu8OverlapUAddr ++;
			pu8OverlapVAddr ++;
		}

		pu8BGTempYAddr += u32BGImgWidth;
		pu8BGTempUAddr += u32BGImgWidth / 2;
		pu8BGTempVAddr += u32BGImgWidth / 2;

		pu8OverlapTempYAddr += u32OverlapWidth;
		pu8OverlapTempUAddr += u32OverlapWidth / 2;
		pu8OverlapTempVAddr += u32OverlapWidth / 2;
	}
}

static void
Overlap_YUV420P(
	S_TEXT_OVERLAP *psTextOverlap,
	uint8_t *pu8BGImg,
	uint32_t u32BGImgWidth,
	uint32_t u32BGImgHeight,
	uint32_t u32PosX,
	uint32_t u32PosY
)
{
	uint8_t *pu8BGYAddr;
	uint8_t *pu8BGYAddr_Next;
	uint8_t *pu8BGUAddr;
	uint8_t *pu8BGVAddr;

	uint8_t *pu8OverlapYAddr;
	uint8_t *pu8OverlapYAddr_Next;
	uint8_t *pu8OverlapUAddr;
	uint8_t *pu8OverlapVAddr;

	uint8_t *pu8BGTempYAddr;
	uint8_t *pu8BGTempUAddr;
	uint8_t *pu8BGTempVAddr;

	uint8_t *pu8OverlapTempYAddr;
	uint8_t *pu8OverlapTempUAddr;
	uint8_t *pu8OverlapTempVAddr;

	uint32_t u32OverlapWidth = psTextOverlap->u32TextImgWidth;
	uint32_t u32OverlapHeight = psTextOverlap->u32TextImgHeight;
	uint8_t u8ColorKeyY = psTextOverlap->u32ColorKeyY;
#if 0
	uint8_t u8ColorKeyU = psTextOverlap->u32ColorKeyU;
	uint8_t u8ColorKeyV = psTextOverlap->u32ColorKeyV;
#endif

	uint32_t x, y;

	uint8_t u8Y0;
	uint8_t u8Y1;
	uint8_t u8Y0_Next;
	uint8_t u8Y1_Next;
	uint8_t u8U;
	uint8_t u8V;

	pu8BGTempYAddr = pu8BGImg + ((u32PosY * u32BGImgWidth) + u32PosX);
	pu8BGTempUAddr = pu8BGImg + u32BGImgWidth * u32BGImgHeight;
	//	pu8BGTempUAddr += (((u32PosY * u32BGImgWidth) + u32PosX) / 4);
	pu8BGTempUAddr += ((u32PosY / 2)  * (u32BGImgWidth / 2)) + (u32PosX / 2);
	pu8BGTempVAddr = pu8BGTempUAddr + ((u32BGImgWidth * u32BGImgHeight) / 4);

	pu8OverlapTempYAddr = psTextOverlap->pu8TextBGImg;
	pu8OverlapTempUAddr = pu8OverlapTempYAddr + (u32OverlapWidth * u32OverlapHeight);
	pu8OverlapTempVAddr = pu8OverlapTempUAddr + ((u32OverlapWidth * u32OverlapHeight) / 4);

	for (y = 0; y < u32OverlapHeight; y += 2) {
		pu8BGYAddr = pu8BGTempYAddr;
		pu8BGUAddr = pu8BGTempUAddr;
		pu8BGVAddr = pu8BGTempVAddr;

		pu8OverlapYAddr = pu8OverlapTempYAddr;
		pu8OverlapUAddr = pu8OverlapTempUAddr;
		pu8OverlapVAddr = pu8OverlapTempVAddr;

		pu8BGYAddr_Next = pu8BGYAddr + u32BGImgWidth;
		pu8OverlapYAddr_Next = pu8OverlapYAddr + u32OverlapWidth;

		for (x = 0; x < u32OverlapWidth; x += 2) {
			u8Y0 = pu8OverlapYAddr[0];
			u8Y1 = 	pu8OverlapYAddr[1];
			u8U = pu8OverlapUAddr[0];
			u8Y0_Next = pu8OverlapYAddr_Next[0];
			u8Y1_Next = pu8OverlapYAddr_Next[1];
			u8V = pu8OverlapVAddr[0];

			if ((u8Y0 != u8ColorKeyY)) {
				pu8BGYAddr[0] = u8Y0;
				pu8BGUAddr[0] = u8U;
				pu8BGVAddr[0] = u8V;
			}

			if ((u8Y1 != u8ColorKeyY)) {
				pu8BGYAddr[1] = u8Y1;
				pu8BGUAddr[0] = u8U;
				pu8BGVAddr[0] = u8V;
			}

			if ((u8Y0_Next != u8ColorKeyY)) {
				pu8BGYAddr_Next[0] = u8Y0_Next;
				pu8BGUAddr[0] = u8U;
				pu8BGVAddr[0] = u8V;
			}

			if ((u8Y1_Next != u8ColorKeyY)) {
				pu8BGYAddr_Next[1] = u8Y1_Next;
				pu8BGUAddr[0] = u8U;
				pu8BGVAddr[0] = u8V;
			}

			pu8BGYAddr += 2;
			pu8BGUAddr ++;
			pu8BGVAddr ++;

			pu8OverlapYAddr += 2;
			pu8OverlapUAddr ++;
			pu8OverlapVAddr ++;

			pu8BGYAddr_Next += 2;
			pu8OverlapYAddr_Next += 2;

		}

		pu8BGTempYAddr += u32BGImgWidth * 2;
		pu8BGTempUAddr += u32BGImgWidth / 2;
		pu8BGTempVAddr += u32BGImgWidth / 2;

		pu8OverlapTempYAddr += u32OverlapWidth * 2;
		pu8OverlapTempUAddr += u32OverlapWidth / 2;
		pu8OverlapTempVAddr += u32OverlapWidth / 2;
	}
}


static void
Overlap_YUV420MB(
	S_TEXT_OVERLAP *psTextOverlap,
	uint8_t *pu8BGImg,
	uint32_t u32BGImgWidth,
	uint32_t u32BGImgHeight,
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

	uint32_t x, y, j;
	uint32_t u32OverlapWidth = psTextOverlap->u32TextImgWidth;
	uint32_t u32OverlapHeight = psTextOverlap->u32TextImgHeight;

	uint8_t u8ColorKeyY = psTextOverlap->u32ColorKeyY;
#if 0
	uint8_t u8ColorKeyU = psTextOverlap->u32ColorKeyU;
	uint8_t u8ColorKeyV = psTextOverlap->u32ColorKeyV;
#endif

	uint32_t u32OverlapMB_Width = u32OverlapWidth / ONE_MB_Y_WIDTH;
	uint32_t u32OverlapMB_Height = u32OverlapHeight / ONE_MB_Y_HEIGHT;

	uint32_t u32MB_x, u32MB_y;

	pu8OverlapYAddr = psTextOverlap->pu8TextBGImg;
	pu8OverlapYPlusAddr = pu8OverlapYAddr + ONE_MB_Y_WIDTH;
	pu8OverlapUAddr = pu8OverlapYAddr + psTextOverlap->u32TextImgWidth * psTextOverlap->u32TextImgHeight;
	pu8OverlapVAddr = pu8OverlapUAddr + ONE_MB_UV_WIDTH;

	for (u32MB_y = 0; u32MB_y < u32OverlapMB_Height; u32MB_y ++) {

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

		for (u32MB_x = 0; u32MB_x < u32OverlapMB_Width; u32MB_x ++) {
			for (y = 0; y < ONE_MB_Y_HEIGHT; y += 2) {
				for (x = 0; x < ONE_MB_Y_WIDTH; x += 2) {
					for (j = 0; j < 2; j ++) {
						// check color key
#if 0
						if ((*pu8OverlapYAddr != u8ColorKeyY) ||
								(*pu8OverlapUAddr != u8ColorKeyU) ||
								(*pu8OverlapVAddr != u8ColorKeyV)) {
#else

						//Just compare Y to get batter result. Not sure it is correct.
						if ((*pu8OverlapYAddr != u8ColorKeyY)) {
#endif
							*pu8BGYAddr = *pu8OverlapYAddr;
							*pu8BGUAddr = *pu8OverlapUAddr;
							*pu8BGVAddr = *pu8OverlapVAddr;
						}

						pu8OverlapYAddr ++;
						pu8BGYAddr ++;
					}

					for (j = 0; j < 2; j ++) {
#if 0

						if ((*pu8OverlapYPlusAddr != u8ColorKeyY) ||
								(*pu8OverlapUAddr != u8ColorKeyU) ||
								(*pu8OverlapVAddr != u8ColorKeyV)) {
#else

						//Just compare Y to get batter result. Not sure it is correct.
						if (*pu8OverlapYPlusAddr != u8ColorKeyY) {
#endif
							*pu8BGYPlusAddr = *pu8OverlapYPlusAddr;
							*pu8BGUAddr = *pu8OverlapUAddr;
							*pu8BGVAddr = *pu8OverlapVAddr;
						}

						pu8OverlapYPlusAddr ++;
						pu8BGYPlusAddr ++;
					}

					pu8OverlapUAddr ++;
					pu8OverlapVAddr ++;

					pu8BGUAddr ++;
					pu8BGVAddr ++;

				}

				pu8OverlapYAddr += ONE_MB_Y_WIDTH;
				pu8OverlapYPlusAddr += ONE_MB_Y_WIDTH;
				pu8OverlapUAddr += ONE_MB_UV_WIDTH;
				pu8OverlapVAddr += ONE_MB_UV_WIDTH;

				pu8BGYAddr += ONE_MB_Y_WIDTH;
				pu8BGYPlusAddr += ONE_MB_Y_WIDTH;
				pu8BGUAddr += ONE_MB_UV_WIDTH;
				pu8BGVAddr += ONE_MB_UV_WIDTH;
			}
		}

		u32PosY += ONE_MB_Y_HEIGHT;
	}
}

H_TEXT_OVERLAP
TextOverlap_Init(
	uint32_t u32FontWidth,				//Font width
	uint32_t u32FontHeight,				//Font height
	E_IMGPROC_COLOR_FORMAT eBGColor,	//Background color format
	uint32_t u32MaxTexts,				//Maxinum texts supported
	uint32_t u32ColorKeyY,				//Color key Y componment
	uint32_t u32ColorKeyU,				//Color key U componment
	uint32_t u32ColorKeyV				//Color key V componment
)
{
	uint8_t *pu8TextImg;
	uint8_t *pu8TextBGImg;
	uint32_t u32TextImgSize = u32FontWidth * u32FontHeight * u32MaxTexts * 2;
	S_TEXT_OVERLAP *psTextOverlap;

	if ((eBGColor != eIMGPROC_YUV422) &&
			(eBGColor != eIMGPROC_YUV422P) &&
			(eBGColor != eIMGPROC_YUV420) &&
			(eBGColor != eIMGPROC_YUV420P) &&
			(eBGColor != eIMGPROC_YUV420P_MB)) {
		printf("[TextOverlap]: Background color unsupported \n");
		return eTEXT_OVERLAP_INVALID_HANDLE;
	}

	if (eBGColor == eIMGPROC_YUV420P_MB) {
		if (u32FontWidth % (ONE_MB_Y_WIDTH / 2)) {
			printf("[TextOverlap]: Font width must multiple of %d \n", (ONE_MB_Y_WIDTH / 2));
			return eTEXT_OVERLAP_INVALID_HANDLE;
		}

		if (u32FontHeight % ONE_MB_Y_HEIGHT) {
			printf("[TextOverlap]: Font height must multiple of %d \n", (ONE_MB_Y_HEIGHT));
			return eTEXT_OVERLAP_INVALID_HANDLE;
		}
	}

	pu8TextImg = malloc(u32TextImgSize);

	if (pu8TextImg == NULL) {
		printf("[TextOverlap]: Allocate text image failed\n");
		return eTEXT_OVERLAP_INVALID_HANDLE;
	}

	pu8TextBGImg = malloc(u32TextImgSize);

	if (pu8TextBGImg == NULL) {
		free(pu8TextImg);
		printf("[TextOverlap]: Allocate text image failed\n");
		return eTEXT_OVERLAP_INVALID_HANDLE;
	}

	psTextOverlap = malloc(sizeof(S_TEXT_OVERLAP));

	if (psTextOverlap == NULL) {
		free(pu8TextImg);
		free(pu8TextBGImg);
		printf("[TextOverlap]: Allocate text overlap handle failed\n");
		return eTEXT_OVERLAP_INVALID_HANDLE;
	}

	psTextOverlap->u32FontWidth = u32FontWidth;
	psTextOverlap->u32FontHeight = u32FontHeight;
	psTextOverlap->u32MaxTexts = u32MaxTexts;
	psTextOverlap->pu8TextImg = pu8TextImg;
	psTextOverlap->pu8TextBGImg = pu8TextBGImg;
	psTextOverlap->u32ColorKeyY = u32ColorKeyY;
	psTextOverlap->u32ColorKeyU = u32ColorKeyU;
	psTextOverlap->u32ColorKeyV	= u32ColorKeyV;

	return (H_TEXT_OVERLAP)psTextOverlap;
}

int32_t
TextOverlap_Overlap(
	H_TEXT_OVERLAP hTextOverlap,
	uint8_t *apu8TextAddr[],
	uint32_t u32NumTexts,
	uint8_t *pu8BGImg,
	E_IMGPROC_COLOR_FORMAT eBGColor,	//Background color format
	uint32_t u32BGImgWidth,
	uint32_t u32BGImgHeight,
	uint32_t u32PosX,
	uint32_t u32PosY
)
{
	if (hTextOverlap == eTEXT_OVERLAP_INVALID_HANDLE)
		return eTEXT_OVERLAP_ERROR_INVALID_HANDLE;

	S_TEXT_OVERLAP *psTextOverlap = (S_TEXT_OVERLAP *)hTextOverlap;

	if (u32NumTexts > psTextOverlap->u32MaxTexts)
		return eTEXT_OVERLAP_ERROR_SIZE;

	if ((u32PosY + psTextOverlap->u32FontHeight) > u32BGImgHeight)
		return eTEXT_OVERLAP_ERROR_SIZE;

	if (u32PosX & 0x01)
		u32PosX --;

	if ((eBGColor == eIMGPROC_YUV420) || (eBGColor == eIMGPROC_YUV420P))
		if (u32PosY & 0x01)
			u32PosY --;

	if (eBGColor == eIMGPROC_YUV420P_MB) {

		uint32_t u32TextImgWidth = psTextOverlap->u32FontWidth * u32NumTexts;

		if (u32TextImgWidth & (ONE_MB_Y_WIDTH - 1)) {
			printf("[TextOverlap]: u32NumTexts must align to marco block boundary\n");
			return eTEXT_OVERLAP_ERROR_MB_BOUNDARY;
		}

		if (u32PosX & (ONE_MB_Y_WIDTH - 1)) {
			printf("[TextOverlap]: u32PosX must align to marco block boundary\n");
			return eTEXT_OVERLAP_ERROR_MB_BOUNDARY;
		}

		if (u32PosY & (ONE_MB_Y_HEIGHT - 1)) {
			printf("[TextOverlap]: u32PosY must align to marco block boundary\n");
			return eTEXT_OVERLAP_ERROR_MB_BOUNDARY;
		}
	}

	//Pack YUV422 text image
	PackTextImg(psTextOverlap, apu8TextAddr, u32NumTexts);

	//Color transfer to BG color
	ColorTrans(psTextOverlap, eBGColor);

	//Overlap image
	if (eBGColor == eIMGPROC_YUV420P_MB)
		Overlap_YUV420MB(psTextOverlap, pu8BGImg, u32BGImgWidth, u32BGImgHeight, u32PosX, u32PosY);
	else if (eBGColor == eIMGPROC_YUV422)
		Overlap_YUV422(psTextOverlap, pu8BGImg, u32BGImgWidth, u32BGImgHeight, u32PosX, u32PosY);
	else if (eBGColor == eIMGPROC_YUV422P)
		Overlap_YUV422P(psTextOverlap, pu8BGImg, u32BGImgWidth, u32BGImgHeight, u32PosX, u32PosY);
	else if (eBGColor == eIMGPROC_YUV420)
		Overlap_YUV420(psTextOverlap, pu8BGImg, u32BGImgWidth, u32BGImgHeight, u32PosX, u32PosY);
	else if (eBGColor == eIMGPROC_YUV420P)
		Overlap_YUV420P(psTextOverlap, pu8BGImg, u32BGImgWidth, u32BGImgHeight, u32PosX, u32PosY);

	return eTEXT_OVERLAP_ERROR_NONE;
}

void
TextOverlap_UnInit(
	H_TEXT_OVERLAP *phTextOverlap
)
{
	if (phTextOverlap == NULL)
		return;

	S_TEXT_OVERLAP *psTextOverlap = (S_TEXT_OVERLAP *)*phTextOverlap;

	if (psTextOverlap == NULL)
		return;

	free(psTextOverlap->pu8TextImg);
	free(psTextOverlap->pu8TextBGImg);
	free(psTextOverlap);

	*phTextOverlap = eTEXT_OVERLAP_INVALID_HANDLE;
}




