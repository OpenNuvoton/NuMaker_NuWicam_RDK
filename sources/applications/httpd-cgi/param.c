// ----------------------------------------------------------------
// Copyright (c) 2012 Nuvoton Technology Corp. All rights reserved.
// ----------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <stdint.h>

#include "common.h"
#include "nuwicam.h"

char * g_pcInputData=NULL;
char * g_pcOuputData=NULL;

char * g_szOuputData[4096]={0};
int g_i32RetCode = 0;


typedef enum {
        eCONFIG_GROUP_UNKNOWN   = -1,
        eCONFIG_GROUP_WIFI,
        eCONFIG_GROUP_STREAM,
        eCONFIG_GROUP_CNT,
} E_CONFIG_GROUP;

static char *s_astrConfigFile[eCONFIG_GROUP_CNT] = {
	DEF_CONF_PATH_NUWICAM_WIFI,
	DEF_CONF_PATH_NUWICAM_STREAM,
	NULL,
};

static char *g_astrGroupName[] = {
	"wifi",
	"stream",
	NULL,
};

static char *g_astrWifiParam[] = {
	"BOOTPROTO",
	"IPADDR",
	"GATEWAY",
	"SSID",
	"AUTH_MODE",
	"ENCRYPT_TYPE",
	"AUTH_KEY",
	"WPS_TRIG_KEY",
	"AP_SSID",
	"AP_CHANNEL",
	"AP_AUTH_KEY",
	"AP_CHANNEL",
	NULL,
};

static char *g_astrStreamParam[] = {
	"VINWIDTH",
	"VINHEIGHT",
	"JPEGENCWIDTH",
	"JPEGENCHEIGHT",
	"BITRATE",
	NULL,
};

static char **g_astrParamGroup[] = {
	g_astrWifiParam,
	g_astrStreamParam,
};


int
ReadParamStr(
	E_CONFIG_GROUP	eConfigGroup,
	char			*strParamName,
	char			*strParamValue,
	int				iParamByteSize		// Byte size of strParamValue
)
{
	char	*strConfigFilePath	= s_astrConfigFile[eConfigGroup];
	FILE	*fConfig = fopen(strConfigFilePath, "r");

	char	*pTmpPtr	= NULL,
				pReadBuf[256];

	int		iReadBytes;

	if (strParamName == NULL || strParamValue == NULL || iParamByteSize == 0)
		return -1;


	if (strConfigFilePath == NULL)
		return -2;


	if (fConfig == NULL)
		return -3;

	memset(strParamValue, 0, iParamByteSize);

	while ((iReadBytes = ReadLine(pReadBuf, sizeof(pReadBuf), fConfig)) > 0) {
		if ((pTmpPtr = strstr(pReadBuf, strParamName)) == pReadBuf) {
			pTmpPtr += strlen(strParamName);

			if (strstr(strParamName, "SSID") != NULL)
				sscanf(pTmpPtr, " \"%[^\"]s", strParamValue);
			else
				sscanf(pTmpPtr, " %s", strParamValue);
			break;
		}
	}

	fclose(fConfig);
	
	return 0;
}	// ReadParamStr

int
WriteParamStr(
	E_CONFIG_GROUP	eConfigGroup,
	char			*strParamName,
	char			*strParamValue
)
{
	int		iRet = 0;
	char	*strConfigFilePath;
	char    strNewConfigFilePath[128];

	if (strParamName == NULL || strParamValue == NULL)
		return -1;

	strConfigFilePath = s_astrConfigFile[eConfigGroup];
	sprintf(strNewConfigFilePath, "%s.new",  strConfigFilePath );

	{
		FILE	*fConfig		= fopen(strConfigFilePath, "r"),
					   *fNewConfig		= fopen(strNewConfigFilePath, "w+");
		char	*pTmpPtr		= NULL,	   pReadBuf[256];
		int		iReadBytes;

		if (fConfig == NULL || fNewConfig == NULL) {
			return -1;
		}
		while ((iReadBytes = ReadLine(pReadBuf, sizeof(pReadBuf), fConfig)) > 0) {
			if (strstr(pReadBuf, strParamName) == pReadBuf) {
				if (strstr(strParamName, "SSID") != NULL)
					fprintf(fNewConfig, "%s \"%s\"\n", strParamName, strParamValue);
				else
					fprintf(fNewConfig, "%s %s\n", strParamName, strParamValue);
			}
			else
				fprintf(fNewConfig, "%s", pReadBuf);
		}
		fclose(fConfig);
		fclose(fNewConfig);
		remove(strConfigFilePath);
		rename(strNewConfigFilePath, strConfigFilePath);
		sync();
	}

	return iRet;
}	// WriteParamStr

int configs_list_all ( E_CONFIG_GROUP eConfigGroup  )
{
	char	*pTmp = NULL;
	int		iGrpIdx;

	int 	iIdx = 0;
	char 	strParamValue[128];
	char 	pBuf[128];

	while ( g_astrParamGroup[eConfigGroup][iIdx] != NULL ) {

		memset(strParamValue, 0, sizeof(strParamValue));
		ReadParamStr(eConfigGroup, g_astrParamGroup[eConfigGroup][iIdx], strParamValue, sizeof(strParamValue));

		if ((pTmp = strrchr(g_astrParamGroup[eConfigGroup][iIdx], '.')) == NULL) {
			pTmp = g_astrParamGroup[eConfigGroup][iIdx];
			sprintf(pBuf, "\"%s\": \"%s\"", pTmp, strParamValue);
		}
		else {
			sscanf(g_astrParamGroup[eConfigGroup][iIdx], "%*[^\[][%d]%*s", &iGrpIdx);
			pTmp = pTmp + 1;
			sprintf(pBuf, "\"%s_%d\": \"%s\"", pTmp, iGrpIdx, strParamValue);
		}

		strcat ( g_szOuputData, pBuf );
		
		iIdx++;

		if (g_astrParamGroup[eConfigGroup][iIdx] != NULL)
			strcat ( g_szOuputData, ", " );
	}
	
	if ( g_szOuputData[0] == '\0' )
		return -1;
	
	return 0;
}

E_CONFIG_GROUP configs_search_groupname (char* pHeadPtr)
{
	int i;
	char *pPtr = NULL;
	for ( i=0 ;i <eCONFIG_GROUP_CNT ;i++ ) 
		if ( (pPtr=strstr(pHeadPtr, g_astrGroupName[i])) != NULL )
			return i;			
	return eCONFIG_GROUP_UNKNOWN;
	
}

int configs_update ( char * pcStr )
{
	char *pHeadPtr = NULL;
	int iRet = -1;
	
	E_CONFIG_GROUP	eGroupID = eCONFIG_GROUP_UNKNOWN ;
	
	if ((pHeadPtr = strstr(pcStr, "group=")) != NULL) {
		pHeadPtr += strlen("group=");
		if ( (eGroupID=configs_search_groupname ( pHeadPtr )) != eCONFIG_GROUP_UNKNOWN )
		{
			char *pSavePtr = NULL;
			char *pToken = strtok_r(pHeadPtr, "&", &pSavePtr);
			char *pParamName, *pParamValue;
			iRet=0;
			while (pToken != NULL) {
				pParamName = strtok(pToken, "=");
				pParamValue = pParamName + strlen(pParamName) + 1;
				
				//printf("pParamName: %s, pParamValue: %s\r\n\r\n", pParamName, pParamValue);

				if ( WriteParamStr(eGroupID, pParamName, pParamValue) != 0 )
				{
					iRet=-1;
					break;
				}
				pToken = strtok_r(NULL, "&", &pSavePtr);
			}
		}
	}
	return iRet;
}

int configs_list ( char * pcStr )
{
	char *pHeadPtr = NULL;
	E_CONFIG_GROUP	eGroupID = eCONFIG_GROUP_UNKNOWN ;
	if ((pHeadPtr = strstr(pcStr, "group=")) != NULL) {
		pHeadPtr += strlen("group=");
		if ( (eGroupID=configs_search_groupname ( pHeadPtr )) != eCONFIG_GROUP_UNKNOWN )
			return configs_list_all ( eGroupID );		
	}	
	return eCONFIG_GROUP_UNKNOWN;
}

int configs_process ( char * pcStr )
{
	int ret = -1;
	
	char *pHeadPtr = NULL;

	if ((pHeadPtr = strstr(pcStr, "action=")) != NULL) {
		pHeadPtr += strlen("action=");
		if (strstr(pHeadPtr, "list") != NULL) {
			pHeadPtr += strlen("list&");
			if ( configs_list(pHeadPtr) != eCONFIG_GROUP_UNKNOWN )
				g_pcOuputData = g_szOuputData ;
		}
		else if (strstr(pHeadPtr, "update") != NULL) {
			pHeadPtr += strlen("update&");
			ret = configs_update(pHeadPtr);
				
		}
	}

	g_i32RetCode = ret;
	
	if ( g_pcOuputData == g_szOuputData )
		ret = 0 ;
		
	return ret;
}

void cgi_response ( void )
{
	printf ( "Content-type: application/json\r\n\r\n" );
	printf ( "{" );
	if ( g_pcOuputData != NULL )
		printf ( "%s", g_pcOuputData );
	else
		printf("\"value\": \"%d\"", g_i32RetCode );	
	
	printf ( "}\r\n\r\n" );
	
}

/*
 * Main routine
 */
int
main(int argc, char **argv)
{
	char* method = getenv("REQUEST_METHOD");

	g_pcInputData  	= get_cgi_data(stdin, method);
	
	g_pcInputData 	= strdup(g_pcInputData);
	
	configs_process(g_pcInputData);
	
	cgi_response();
	
	if ( g_pcInputData )
		free(g_pcInputData);
	
	return 0;
}
