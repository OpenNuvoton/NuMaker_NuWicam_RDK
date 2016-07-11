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


int 	// > 0: string data, 0: only newline, < 0: EOF
ReadLine(
	char	*pBuf,
	int		iBufByteSize,
	FILE	*stream
)
{
	int     chGet,
			iReadBytes = 0;

	while ((chGet = fgetc(stream)) != EOF) {
		if (chGet == '\r')
			chGet = '\n';

		pBuf[iReadBytes++] = chGet;

		if (chGet == '\n')
			break;

		if (iReadBytes == iBufByteSize)
			break;
	}

	if (iReadBytes == 0 && chGet == EOF)
		return -1;

	pBuf[iReadBytes] = 0;

	return iReadBytes;
}	// ReadLine

char* get_cgi_data(FILE* fp, char* method)
{
	char* input;
	if (strcmp(method, "GET") == 0)  /**< GET method */
	{
		input = getenv("QUERY_STRING");
		return input;
	}
#if 0	
	else if (strcmp(method, "POST") == 0)  /**< POST method */
	{
		int i=0;
		int size=1024;
		int len;
		len = atoi(getenv("CONTENT_LENGTH"));
		input = (char*)malloc(sizeof(char) * (size+1));
		if (len == 0)
		{
			input[0]="\"";
			return input;
		}
		while (1)
		{
			input[i] = (char)fgetc(fp);
			if (i == size)
			{
				input[i+1] ="\"";
				return input;
			}
			--len;
			if (feof(fp) || (!(len)))
			{
				i++;
				input[i] ="\"";
				return input;
			}
			i++;
		}
	}
#endif
	return NULL;
}
