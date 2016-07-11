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

#ifndef __UTILS_H___
#define __UTILS_H___

#include <stdint.h>
#include <getopt.h>

typedef enum {
	eKERNEL_2_6_17_14,
	eKERNEL_2_6_35_4,
	eKERNEL_UNKNOW,
} E_KERNEL_VER;

#define ABS(a) (((a) < 0) ? -(a) : (a))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define LENGTH_OF(x) (sizeof(x)/sizeof(x[0]))

// Boolean type
#if !defined(__cplusplus) && !defined(NM_NBOOL)
	#if defined(bool) && (false != 0 || true != !false)
		#warning bool is redefined: false(0), true(!0)
	#endif

	#undef	bool
	#undef	false
	#undef	true
	#define bool	uint8_t
	#define false	0
	#define true	(!false)
#endif // #if !defined(__cplusplus) && !defined(NM_NBOOL)


//int
uint64_t
utils_get_ms_time();

E_KERNEL_VER
utils_get_kernel_version();


int
CountOfBitOne(
	uint32_t	u32Bitmask
);

#endif	// __UTILS_H___
