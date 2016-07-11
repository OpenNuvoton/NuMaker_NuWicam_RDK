#ifndef _TVP_DECODER_H
#define _TVP_DECODER_H

typedef enum {
	eTVP_PAL,
	eTVP_NTSC,
	eTVP_CNT		
} E_TVP_MODE;

extern E_TVP_MODE g_eTVPMode;
extern int CROP_START_X;
extern int CROP_START_Y;

E_TVP_MODE tvp_getmode(void);

#endif	//_TVP_DECODER_H
