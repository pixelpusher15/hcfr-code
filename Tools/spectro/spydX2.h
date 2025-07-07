#ifndef SPYDX2_H

/* 
 * Argyll Color Management System
 *
 * ColorVision Spyder X2 related software.
 *
 * Author: Graeme W. Gill
 * Date:   30/8/2024
 *
 * Copyright 2006 - 2024, Graeme W. Gill
 * All rights reserved.
 *
 * (Based on spydX.h)
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/* 
   If you make use of the instrument driver code here, please note
   that it is the author(s) of the code who are responsibility
   for its operation. Any problems or queries regarding driving
   instruments with the Argyll drivers, should be directed to
   the Argyll's author(s), and not to any other party.

   If there is some instrument feature or function that you
   would like supported here, it is recommended that you
   contact Argyll's author(s) first, rather than attempt to
   modify the software yourself, if you don't have firm knowledge
   of the instrument communicate protocols. There is a chance
   that an instrument could be damaged by an incautious command
   sequence, and the instrument companies generally cannot and
   will not support developers that they have not qualified
   and agreed to support.
 */

#include "inst.h"

#ifdef __cplusplus
	extern "C" {
#endif

/* Note: update spydX2_interp_error() and spydX2_interp_code() in spydX2.c */
/* if anything of these #defines are added or subtracted */

typedef int spydX2_code;

#define SPYDX2_OK   				0x00

/* Fake Error codes */
#define SPYDX2_INTERNAL_ERROR		0x61		/* Internal software error */
#define SPYDX2_COMS_FAIL			0x62		/* Communication failure */
#define SPYDX2_UNKNOWN_MODEL		0x63		/* Not an spydX2lay */
#define SPYDX2_DATA_PARSE_ERROR  	0x64		/* Read data parsing error */


#define SPYDX2_NO_COMS				0x80		/* No communications when it's needed */
#define SPYDX2_CIX_MISMATCH			0x81		/* Got different calibration than asked for */

/* Most 8 bit instrument error codes are unknown */
#define SPYDX2_BAD_PARAM			0x01		/* Parameter out of range ? */


/* Internal error codes */
#define SPYDX2_INT_CAL_SAVE         0xE009		/* Saving calibration to file failed */
#define SPYDX2_INT_CAL_RESTORE      0xE00A		/* Restoring calibration to file failed */
#define SPYDX2_INT_CAL_TOUCH        0xE00B		/* Touching calibration to file failed */
 
/* Extra native calibration info */
typedef struct {
	int ix;					/* Native index */

	int v1;					/* Magic 8bit value from get_mtx and supplied to get_setup cmd */
							/* Seems to be gain setting (2 bits), but there are no */
							/* setup entries for gains other than 3 (== 64x) */
							/* This is the same for all 4 calibrations and doesn't vary */
							/* with light level. */

	int v2;					/* Magic 16bit value from get_mtx and supplied to measure cmd. */
							/* This is the integration time in msec ???? Actual time = */
							/* 2.8 * floor(v2/2.8), maximum value = 719. Default = 714 */
							/* This is the same for all 4 calibrations and doesn't vary */
							/* with light level. */

	int v3;					/* Magic value returned and not used ? */

	int v4[6];				/* sensor indexes ? */ 

	double mat[3][6];		/* Native calibration matrix */
	double gain[3];			/* XYZ gain value */
	double off[3];			/* XYZ offset value */

	int s1;					/* 8 bit value from get_setup (same as v1, sets gain) */
	int s2;					/* 16 bit value from get_setup (same as v2, sets gain) */
	int s3[6];				/* Sensor indexes */
	int s4[6];				/* Values from get_setup and supplied to measure cmd */  
							/* (Typically 0xbf, 0x9f or similiar) */
	int s5[6];				/* Values from get_setup, sensor zero values ? */
							/* (Typically 0x01) */

} SpX2calinfo;

#define SPYDX2_NOCALIBS 5

/* SPYDX2 communication object */
struct _spydX2 {
	INST_OBJ_BASE

	inst_mode mode;				/* Currently selected mode (emis/ambient/etc.) */

	inst_opt_type trig;			/* Reading trigger mode */

	unsigned int hwvn[2];		/* Harware major, Minor version numbers */

								/* Initial SpyderX2 = ?.?? */


	char    serno[9];			/* 8:8xB  Serial number as zero terminated string */

	/* Computed factors and state */
	inst_disptypesel *dtlist;	/* Display Type list */
	int ndtlist;				/* Number of valid dtlist entries */

	SpX2calinfo cinfo[SPYDX2_NOCALIBS];	/* cal & meas setup info indexed by native ix */
	
	int ix;						/* current native cal index */
	int cbid;					/* current calibration base ID, 0 if not a base */
	int ucbid;					/* Underlying base ID if being used for matrix, 0 othewise */
	disptech dtech;				/* Display technology enum */

	double ccmat[3][3];			/* Current colorimeter correction matrix, unity if none */

								// Hmm. This might be per calibration ...
	int bcal_done;				/* Black offset calibration is valid */
	int bcal[6];				/* Black offset calibration values */
	time_t bdate;				/* Date/time of last black calibration */

	int noinitcalib;		 	/* Don't do initial calibrate, or we've done initial calib.  */
	int lo_secs;				/* Seconds since last opened (from calibration file mod time) */ 

}; typedef struct _spydX2 spydX2;

/* Constructor */
extern spydX2 *new_spydX2(icoms *icom, instType itype);

#ifdef __cplusplus
	}
#endif

#define SPYDX2_H
#endif /* SPYDX2_H */
