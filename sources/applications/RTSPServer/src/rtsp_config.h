// ----------------------------------------------------------------
// Copyright (c) 2013 Nuvoton Technology Corp. All rights reserved.
// ----------------------------------------------------------------

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
// MA 02110-1301, USA.

#ifndef __RTSP_CONFIG_H___
#define __RTSP_CONFIG_H___

#define DEF_MAX_MJPEG_CAM	4
#define DEF_MAX_H264_CAM	2

typedef enum {
	eRTSP_AUDIOMASK_NONE	= 0x00000000,
	eRTSP_AUDIOMASK_ALAW	= 0x00000001,
	eRTSP_AUDIOMASK_ULAW	= 0x00000002,
	eRTSP_AUDIOMASK_DVIADPCM	= 0x00000004,
	eRTSP_AUDIOMASK_AAC		= 0x00000008,
	eRTSP_AUDIOMASK_G726	= 0x00000010,
	eRTSP_AUDIOMASK_MP3	= 0x00000020,
} E_RTSP_AUDIOMASK;

typedef struct rtsp_config_t {
	uint32_t	m_uiFrameSize;		//frame size of memory heap
	uint32_t	m_uiFrameCnt;		//frame count of memory heap. Total memory heap size = m_uiFrameSize * m_uiFrameCnt
	uint32_t	m_uiMJPGCamBitRate;		//mjpeg cam bitrate
	char 		*m_szMJPGCamPath;
	char 		*m_szH264CamPath;
	E_RTSP_AUDIOMASK m_eMJPGCamAudio;
	E_RTSP_AUDIOMASK m_eH264CamAudio;
	uint32_t	m_uiNumMJPEGCams;
	uint32_t	m_uiNumH264Cams;
	uint32_t	m_uiRTPDataTransSize;
	char		*m_szAuthRealm;
	char		*m_szAuthUserName;
	char		*m_szAuthPassword;
	char 		*m_szAudioStreamPath;
	E_RTSP_AUDIOMASK m_eAudioStreamAudio;
	uint32_t	m_auiH264MaxConnNum[DEF_MAX_H264_CAM];
	uint32_t	m_auiMJPGMaxConnNum[DEF_MAX_MJPEG_CAM];
	uint32_t 	m_uiVinWidth;
	uint32_t 	m_uiVinHeight;
	uint32_t 	m_uiJpegWidth;
	uint32_t 	m_uiJpegHeight;	
} S_RTSP_CONFIG;

#endif	// __PLUGIN_RTSP_H___
