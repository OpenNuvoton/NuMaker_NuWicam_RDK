// ----------------------------------------------------------------
// Copyright (c) 2012 Nuvoton Technology Corp. All rights reserved.
// ----------------------------------------------------------------

/*******************************************************************************
#                                                                              #
#      MJPG-streamer allows to stream JPG frames from an input-plugin          #
#      to several output plugins                                               #
#                                                                              #
#      Copyright (C) 2007 Tom Stöveken                                         #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/types.h>
#include <string.h>
#include <fcntl.h>
#include <wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/utsname.h>

#include "utils.h"

/******************************************************************************
Description.:
Input Value.:
Return Value:
******************************************************************************/

//int
uint64_t
utils_get_ms_time()
{
	struct timespec sTimeSpec;
	uint64_t u64CurTime;
	clock_gettime(CLOCK_MONOTONIC, &sTimeSpec);
	u64CurTime = ((uint64_t)sTimeSpec.tv_sec * 1000) + ((uint64_t)sTimeSpec.tv_nsec / 1000000);

	return u64CurTime;
}

E_KERNEL_VER
utils_get_kernel_version()
{
	struct utsname  uts;

	uname(&uts);

	if (strstr(uts.release, "2.6.17.14")) {
		return eKERNEL_2_6_17_14;
	}

	if (strstr(uts.release, "2.6.35.4")) {
		return eKERNEL_2_6_35_4;
	}

	return eKERNEL_UNKNOW;
}

