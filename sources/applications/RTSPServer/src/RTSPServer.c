/*
 * RTMPStreamer.c
 * Copyright (C) nuvoTon
 * 
 * RTSPServer.c is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * RTMPStreamer.c is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <fcntl.h>
#include <time.h>

#include "spook.h"

#define DEF_JPEG_URL_PATH "/cam1/mpeg4"
#define DEF_AUDIO_ONLY_URL_PATH "/audio"

#define DEF_AUTH_USERNAME ""	//"admin"
#define DEF_AUTH_PASSWORD ""	//"admin"
#define DEF_MAX_ALLOWED_STREAM_CONNECTION 2

volatile static S_RTSP_CONFIG s_sConfig = {
	.m_uiFrameSize = 80000,
	.m_uiFrameCnt = 8,
	.m_uiMJPGCamBitRate = 4000000,
	.m_szMJPGCamPath = NULL,
	.m_szH264CamPath = NULL,
	.m_eMJPGCamAudio = eRTSP_AUDIOMASK_ALAW,
	.m_eH264CamAudio = eRTSP_AUDIOMASK_NONE,
	.m_uiNumMJPEGCams = 1,
	.m_uiNumH264Cams = 0,
	.m_uiRTPDataTransSize = 1400,
	.m_szAuthRealm = NULL,
	.m_szAudioStreamPath = NULL,
	.m_eAudioStreamAudio = eRTSP_AUDIOMASK_NONE,
	.m_uiVinWidth = 640,
	.m_uiVinHeight = 480,
	.m_uiJpegWidth = 640,
	.m_uiJpegHeight = 480,
};

static void ShowUsage()
{
	printf("RTSPServer [options]\n");
	printf("-w video-in width. Must multiple of 8 \n");
	printf("-d video-in height. Must multiple of 8 \n");
	printf("-i Jpeg encode width \n");
	printf("-e Jpeg encode height\n");
	printf("-b Jpeg bit rate \n");
}

int
main(int argc, char **argv)
{
	int32_t i;
	int32_t i32Opt;

	// Parse options
	while ((i32Opt = getopt(argc, argv, "w:d:i:e:b:h")) != -1) {
		switch (i32Opt) {
			case 'w':
			{
				uint32_t u32Width = atoi(optarg); 
				
				if(u32Width){
					if(u32Width % 8){
						u32Width =  u32Width & ~7;
						printf("The video-in width must multiple of 8. Set video-in width to %d \n", u32Width);
					}
					s_sConfig.m_uiVinWidth = u32Width;
				}
				else{
					printf("Specified an invalid video-in width %d \n", u32Width);
					exit(-1);
				}
			}
			break;
			case 'd':
			{
				uint32_t u32Height = atoi(optarg);
				
				if(u32Height){
					if(u32Height % 8){
						u32Height =  u32Height & ~7;
						printf("The video-in height must multiple of 8. Set video-in height to %d \n", u32Height);
					}
					s_sConfig.m_uiVinHeight = u32Height;
				}
				else{
					printf("Specified an invalid video-in height %d \n", u32Height);
					exit(-1);
				}
			}
			break;
			case 'i':
			{
				uint32_t u32Width = atoi(optarg); 

				if(u32Width){
					if(u32Width % 8){
						u32Width =  u32Width & ~7;
						printf("The Jpeg encode width must multiple of 8. Set Jpeg encode width to %d \n", u32Width);
					}
					s_sConfig.m_uiJpegWidth = u32Width;
				}
				else{
					printf("Specified an invalid jpeg encode width %d \n", u32Width);
					exit(-1);
				}
			}
			break;
			case 'e':
			{
				uint32_t u32Height = atoi(optarg);
				
				if(u32Height){
					if(u32Height % 8){
						u32Height =  u32Height & ~7;
						printf("The Jpeg encode height must multiple of 8. Set Jpeg encode height to %d \n", u32Height);
					}
					s_sConfig.m_uiJpegHeight = u32Height;
				}
				else{
					printf("Specified an invalid Jpeg encode height %d \n", u32Height);
					exit(-1);
				}
			}
			break;
			case 'b':
			{
				uint32_t u32BitRate = atoi(optarg);
				
				if(u32BitRate)
					s_sConfig.m_uiMJPGCamBitRate = u32BitRate;				
			}
			break;
			case 'h':
			default:
				ShowUsage();
				return 0;
		}
	}

	for (i = 0; i < DEF_MAX_MJPEG_CAM; i++) {
		s_sConfig.m_auiMJPGMaxConnNum[i] = DEF_MAX_ALLOWED_STREAM_CONNECTION;
	}

//	for (i = 0; i < DEF_MAX_H264_CAM; i++) {
//		s_sConfig.m_auiH264MaxConnNum[i] = 1;
//	}

	if (s_sConfig.m_szMJPGCamPath == NULL)
		s_sConfig.m_szMJPGCamPath = strdup(DEF_JPEG_URL_PATH);

	if (s_sConfig.m_szAudioStreamPath == NULL)
		s_sConfig.m_szAudioStreamPath = strdup(DEF_AUDIO_ONLY_URL_PATH);

	if (s_sConfig.m_szAuthUserName == NULL)
		s_sConfig.m_szAuthUserName = strdup(DEF_AUTH_USERNAME);

	if (s_sConfig.m_szAuthPassword == NULL)
		s_sConfig.m_szAuthPassword = strdup(DEF_AUTH_PASSWORD);


	if (spook_init() < 0) {
		printf("ERROR: rtsp server init failed\n");
		return - 2;
	}

	if (spook_config((S_RTSP_CONFIG *) &s_sConfig) < 0) {
		printf("ERROR: rtsp server config failed\n");
		return - 3;
	}

	spook_run();

	return 0;
}
