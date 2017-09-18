/****************************************************************
 *                                                              *
 * Copyright (c) Nuvoton Technology Corp.  All rights reserved. *
 *                                                              *
 ****************************************************************/
#ifndef __IMG_PROC_COMM_H__
#define __IMG_PROC_COMM_H__

#include <stdint.h>


#ifndef ERRCODE
	#define ERRCODE int32_t
#endif

#ifndef BOOL
	#define BOOL int32_t
	#define TRUE (1)
	#define FALSE (0)
#endif

#define IMGPROC_ERR			0x80100000
#define BLT_ERR				0x80200000
#define VPE_ERR				0x80400000
#define IMGSPECEFT_ERR		0x80800000


#endif
