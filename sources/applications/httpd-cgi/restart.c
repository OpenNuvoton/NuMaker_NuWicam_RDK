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
        eCONFIG_GROUP_BOARD,
        eCONFIG_GROUP_STREAM,        
        eCONFIG_GROUP_CNT,
} E_CONFIG_GROUP;

static char *g_astrGroupName[] = {
	"wifi",
	"board",
	"stream",
	NULL,
};

E_CONFIG_GROUP configs_search_groupname (char* pHeadPtr)
{
	int i;
	char *pPtr = NULL;
	for ( i=0 ;i <eCONFIG_GROUP_CNT ;i++ ) 
		if ( (pPtr=strstr(pHeadPtr, g_astrGroupName[i])) != NULL )
			return i;			
	return eCONFIG_GROUP_UNKNOWN;
	
}

int configs_process ( char * pcStr )
{
	char *pHeadPtr = NULL;
	int ret=-1;
	E_CONFIG_GROUP	eGroupID = eCONFIG_GROUP_UNKNOWN ;
	if ((pHeadPtr = strstr(pcStr, "group=")) != NULL) {
		pHeadPtr += strlen("group=");
		if ( (eGroupID=configs_search_groupname ( pHeadPtr )) != eCONFIG_GROUP_UNKNOWN )
		{
			switch (eGroupID) 
			{
				case eCONFIG_GROUP_WIFI:
					ret=system("/mnt/nuwicam/bin/go_networking.sh");
					break;
				case eCONFIG_GROUP_BOARD:
					ret=system("export PATH=/sbin:$PATH; sync;sync;sync; reboot");
					break;
				case eCONFIG_GROUP_STREAM:				
					ret=system("killall -9 RTSPServer");
					break;
			}
		}
	}
	g_i32RetCode = ret;
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
