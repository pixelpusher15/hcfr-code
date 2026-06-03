
/* 
 * Argyll Color Management System
 *
 * Datacolor Spyder X2, Spyder 2024 related software.
 *
 * Author: Graeme W. Gill
 * Date:   30/8/2024
 *
 * Copyright 2006 - 2025, Graeme W. Gill
 * All rights reserved.
 *
 * (Based on spydX.c)
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

/* TTBD:

	Add env variable to make 2024 use old measurement commands ?
	If so, when in old mode enable optional black cal.

*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <math.h>
#include <fcntl.h>
#ifndef SALONEINSTLIB
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#else /* SALONEINSTLIB */
#include "sa_config.h"
#include "numsup.h"
#endif /* SALONEINSTLIB */
#include "cgats.h"
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"
#include "rspec.h"
#include "spydX2.h"

#undef ENABLE_BLACK_CAL		/* [und] Force black calibration */
#define DCALTOUT (30 * 60)	/* [30 Minutes] Dark Calibration timeout in seconds */
#define ENABLE_NONVCAL		/* [def] Enable saving calibration state between program runs in a file */

static inst_code spydX2_interp_code(inst *pp, int ec);
static int spydX2_save_calibration(spydX2 *p);
static int spydX2_restore_calibration(spydX2 *p);
static int spydX2_touch_calibration(spydX2 *p);
 

/* ------------------------------------------------------------------------ */
/* Implementation */

/* Interpret an icoms error into a SPYDX2 error */
static int icoms2spydX2_err(int se) {
	if (se != ICOM_OK)
		return SPYDX2_COMS_FAIL;
	return SPYDX2_OK;
}

/* ============================================================ */
/* Low level commands */


#define BUF_SIZE 1024

/* USB Instrument commands */

/* Reset the instrument */
static inst_code
spydX2_reset(
	spydX2 *p
) {
	int se;
	inst_code rv = inst_ok;

	a1logd(p->log, 3, "spydX2_reset: called\n");

	se = p->icom->usb_control(p->icom,
	               IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_INTERFACE,
                   0x02, 2, 0, NULL, 0, NULL, 5.0);

	if (se == ICOM_OK) {
		a1logd(p->log, 6, "spydX2_reset: complete, ICOM code 0x%x\n",se);
	}

	msec_sleep(500);	// ~~~

	return rv;
}

/* Execute a command */
static spydX2_code
spydX2_command(
	spydX2 *p,
	int cmd,				/* Command code */
	unsigned char *send,	/* Payload send buffer */
	unsigned int s_size,	/* Payload send size */
	unsigned char *reply,	/* Payload reply buffer */
	unsigned int r_size,	/* Expected payload reply size */
	int chsum,				/* nz to check checksum */
	double to				/* Timeout in seconds */
) {
	ORD8 buf[BUF_SIZE];
	unsigned int nonce, chnonce;
	int xfrd;				/* Bytes transferred */
	unsigned int iec;		/* Instrument error code */
	int xrlen;				/* Expected receive length */
	int se;

	if ((s_size + 5) > BUF_SIZE
	 || (r_size + 5) > BUF_SIZE)
		error("USB buffer size too small in '%s' line %d\n",__FILE__,__LINE__);
	
	nonce = 0xffff & rand32(0);

	/* Setup send command */
	buf[0] = (unsigned char)cmd;
	write_ORD16_be(buf+1, nonce);
	write_ORD16_be(buf+3, s_size);
	if (s_size > 0)
		memcpy(buf+5, send, s_size);
	
	if (p->log->debug >= 7) {
		a1logd(p->log, 1, "sending:\n");
		adump_bytes(p->log, "  ", buf, 0, 5 + s_size);
	}

	se = p->icom->usb_write(p->icom, NULL, 0x01, buf, 5 + s_size, &xfrd, to);
	
	if (se != 0) {
		a1logd(p->log, 1, "spydX2_command: Command send failed with ICOM err 0x%x\n",se);

		/* Flush any response */
		p->icom->usb_read(p->icom, NULL, 0x81, buf, 5 + r_size, NULL, to);

		return SPYDX2_COMS_FAIL;
	}

	if (xfrd != (5 + s_size)) {
		a1logd(p->log, 1, "spydX2_command: Command sent %d bytes instead of %d\n",
		                                                      xfrd, 5 + s_size);

		/* Flush any response */
		p->icom->usb_read(p->icom, NULL, 0x81, buf, 5 + r_size, NULL, to);

		return SPYDX2_COMS_FAIL;
	}

	/* Read the response */
	a1logd(p->log, 5, "spydX2_command: Reading response\n");

	se = p->icom->usb_read(p->icom, NULL, 0x81, buf, 5 + r_size, &xfrd, to);

	if (p->log->debug >= 7) {
		a1logd(p->log, 1, "received:\n");
		adump_bytes(p->log, "  ", buf, 0, xfrd);
	}

	if (se != 0) {
		a1logd(p->log, 1, "spydX2_command: response read failed with ICOM err 0x%x\n",se);

		return SPYDX2_COMS_FAIL;
	}

	if (xfrd != (5 + r_size)) {
		a1logd(p->log, 1, "spydX2_command: Command got %d bytes instead of %d\n",
		                                                      xfrd, 5 + r_size);
		return SPYDX2_COMS_FAIL;
	}

	/* Check instrument error code */
	if ((iec = read_ORD16_be(buf + 2)) != 0) {
		a1logd(p->log, 1, "spydX2_command: Got instrument error %d\n",iec);
		return SPYDX2_COMS_FAIL;
	}

	/* Check nonce */
	if ((chnonce = read_ORD16_be(buf + 0)) != nonce) {
		a1logd(p->log, 1, "spydX2_command: Nonce mismatch got 0x%x expect 0x%x\n",chnonce,nonce);
		return SPYDX2_COMS_FAIL;
	}

	/* Check expected data length */
	if ((xrlen = read_ORD16_be(buf + 3)) != r_size) {
		a1logd(p->log, 1, "spydX2_command: Reply payload len %d but expect %d\n",xrlen,r_size);
		return SPYDX2_COMS_FAIL;
	}

	if (chsum) {
		unsigned int sum = 0, i;
		for (i = 0; i < (r_size-1); i++)
			sum += buf[5 + i];

		sum &= 0xff;
		if (sum != buf[5 + i]) {
			a1logd(p->log, 1, "spydX2_command: Checksum failed, is 0x%x should be 0x%x\n",sum,buf[5 + i]);
			return SPYDX2_COMS_FAIL;
		}
	}

	/* Return payload */
	if (r_size > 0)
		memcpy(reply, buf + 5, r_size);

	return SPYDX2_OK;
}

/* Get the HW version, Serial number and other instrument info. */
static inst_code
spydX2_getInstInfo(
	spydX2 *p
) {
	ORD8 reply[0x2A], buf[3];
	int se = ICOM_OK;
	inst_code rv = inst_ok;

	a1logd(p->log, 3, "spydX2_getInstInfo: called\n");

	se = spydX2_command(p, 0xC2, NULL, 0, reply, 0x25, 0, 5.0);  

	if (se != SPYDX2_OK) {
		rv = spydX2_interp_code((inst *)p, icoms2spydX2_err(se));
		a1logd(p->log, 6, "spydX2_getInstInfo: failed with ICOM code 0x%x\n",rv);
		return rv;
	}

	/* Major no */
	buf[0] = reply[0];
	buf[1] = '\000';
	p->hwvn[0] = atoi((char *)buf);

	/* Minor no */
	buf[0] = reply[2];
	buf[1] = reply[3];
	buf[2] = '\000';
	p->hwvn[1] = atoi((char *)buf);

	memcpy(p->serno, reply + 4, 8);
	p->serno[8] = '\000';

	a1logd(p->log, 3, "spydX2_getInstInfo got HW '%d.%02d and SN '%s'\n",p->hwvn[0],p->hwvn[1], p->serno);

	p->hlavail = 0;
	p->mxdnp1 = 0;
	p->dnomask = 0;
	if (p->is2024) {
		if (reply[17] == 0x09 && reply[18] == 0x08 && reply[19] == 0x01) {
			p->hlavail = 1;
			p->mxdnp1 = reply[35];					/* Max number of display types + 1 */
			p->dnomask = reply[20] << 8 | reply[21];	/* Display type mask */
			a1logd(p->log, 3, "spydX2_getInstInfo got hlavail %d, mxdnp1 %d, dnomask 0x%x\n",p->hlavail,p->mxdnp1,p->dnomask);
		}
	}

	return rv;
}

/* Get a calibration */
static inst_code
spydX2_getCalibration(
	spydX2 *p
) {
	SpX2calinfo *ci = &p->cinfo[p->ix];
	int i, j;
	ORD8 send[0x1];
	ORD8 reply[0x6C];
	int v0;
	int se = ICOM_OK;
	inst_code rv = inst_ok;

	a1logd(p->log, 3, "spydX2_getCalibration %d: called\n",p->ix);

	send[0] = p->ix;

	se = spydX2_command(p, 0xF6, send, 1, reply, 0x6C, 1, 5.0);  

	if (se != SPYDX2_OK) {
		rv = spydX2_interp_code((inst *)p, icoms2spydX2_err(se));
		a1logd(p->log, 6, "spydX2_getCalibration: failed with ICOM code 0x%x\n",rv);
		return rv;
	}

	v0 = read_ORD8(reply + 0);		/* Confirm cix */

	if (v0 != p->ix) {
		rv = spydX2_interp_code((inst *)p, SPYDX2_CIX_MISMATCH);
		a1logd(p->log, 6, "spydX2_getCalibration cix mismatch: set %d got %d\n",p->ix,v0);
		return rv;
	}

	ci->v1 = read_ORD8(reply + 1);		/* Magic 8 bit value fed to setup command */
	ci->v2 = read_ORD16_be(reply + 2);	/* Magic 16 bit value fed to measure command (int time ?) */

	/* Channel indexes 0..5 */
	for (i = 0; i < 6; i++)
		ci->v4[i] = read_ORD8(reply + 4 + i);

	/* Matrix values */
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 6; j++) {
			ORD32 vv = read_ORD32_le(reply + 10 + (j * 3 + i) * 4);
			ci->mat[i][j] = IEEE754todouble(vv);
		}
	}

	/* Extra values */
	for (j = 0; j < 3; j++) {
		ORD32 vv;
		vv = read_ORD32_le(reply + 82 + (j * 2 + 0) * 4);
		ci->gain[j] = IEEE754todouble(vv);
		vv = read_ORD32_le(reply + 82 + (j * 2 + 1) * 4);
		ci->off[j] = IEEE754todouble(vv);
	}

	ci->v3 = read_ORD8(reply + 106);	/* Magic 8 bit value unused */

	if (p->log->debug >= 3) {
		a1logd(p->log, 3, "spydX2_getCalibration got v1 = %d, v2 = %d, v3 =  %d\n",ci->v1,ci->v2,ci->v3);
		a1logd(p->log, 3, "  v4 = %d %d %d %d %d %d\n",
		       ci->v4[0], ci->v4[1], ci->v4[2], ci->v4[3], ci->v4[4], ci->v4[5]);
		for (j = 0; j < 6; j++)
			for (i = 0; i < 3; i++)
				a1logd(p->log, 3, " mat[%d][%d] = %f\n",i,j,ci->mat[i][j]);

		for (j = 0; j < 3; j++)
			a1logd(p->log, 3, " gain[%d] = %f, off[%d] = %f\n",j,ci->gain[j],j,ci->off[j]);
	}

	return rv;
}

/* Do measurement setup. */
/* This is used before the measurement command. */
/*
	for Spyder X:
	v1 value of 3 works.
	v1 values 0, 1, 2 return all 0xff values that fails checksum.
	v1 value > 3 returns 5 bytes header with error byte = 0x01
	Haven't checked X2.
 */
static inst_code
spydX2_measSettup(
	spydX2 *p
) {
	SpX2calinfo *ci = &p->cinfo[p->ix];
	int i;
	ORD8 send[0x1];
	ORD8 reply[0x16];
	int se = ICOM_OK;
	inst_code rv = inst_ok;

	a1logd(p->log, 3, "spydX2_measSettup %d: called\n",ci->v1);

	send[0] = ci->v1;

	se = spydX2_command(p, 0xF7, send, 1, reply, 0x16, 1, 5.0);  

	if (se != SPYDX2_OK) {
		rv = spydX2_interp_code((inst *)p, icoms2spydX2_err(se));
		a1logd(p->log, 6, "spydX2_measSettup: failed with ICOM code 0x%x\n",rv);
		return rv;
	}

	ci->s1 = read_ORD8(reply + 2);

	if (ci->s1 != ci->v1) {
		rv = spydX2_interp_code((inst *)p, SPYDX2_CIX_MISMATCH);
		a1logd(p->log, 6, "spydX2_measSettup v1 mismatch: set %d got %d\n",ci->v1,ci->s1);
		return rv;
	}
	ci->s2 = read_ORD16_be(reply + 0);

	/* Channel indexes 0..5 */
	for (i = 0; i < 6; i++)
		ci->s3[i] = read_ORD8(reply + 3 + i);

	/* Some sort of per channel values */
	for (i = 0; i < 6; i++)
		ci->s4[i] = read_ORD8(reply + 9 + i);

	for (i = 0; i < 6; i++)
		ci->s5[i] = read_ORD8(reply + 15 + i);

	a1logd(p->log, 3, "spydX2_measSettup got s1 = %d, s2 = %d\n",ci->s1,ci->s2);
	a1logd(p->log, 3, "  s3 = %d %d %d %d %d %d\n",
	       ci->s3[0], ci->s3[1], ci->s3[2], ci->s3[3], ci->s3[4], ci->s3[5]);
	a1logd(p->log, 3, "  s4 = %d %d %d %d %d %d\n",
	       ci->s4[0], ci->s4[1], ci->s4[2], ci->s4[3], ci->s4[4], ci->s4[5]);
	a1logd(p->log, 3, "  s5 = %d %d %d %d %d %d\n",
	       ci->s5[0], ci->s5[1], ci->s5[2], ci->s5[3], ci->s5[4], ci->s5[5]);
	return rv;
}

/* Do a raw measurement. */
/*

	~~ need to check on X2:
	s1 value seems to be gain selector ??
		0 = 1	
		1 = 3.7
		2 = 16
		3 = 64
	Gain ratio's don't seem to be perfect though (i.e. ~ 2% errors ??)

	v2 is integration time in msec, max value 719. 
		i.e. inttime = 2.8 * floor(v2/2.8)
	Calibrated value = 714 = 255 units = 714 msec.

	s2[3] values seem to act like a signed gain trim to an offset value ?


*/
static inst_code
spydX2_Measure(
	spydX2 *p,
	int raw[6]				/* return 4 raw channel values, X,Y,Z,iR */
) {
	SpX2calinfo *ci = &p->cinfo[p->ix];
	int i;
	ORD8 send[0xf];
	ORD8 reply[0xc];
	int se = ICOM_OK;
	inst_code rv = inst_ok;

	a1logd(p->log, 3, "spydX2_Measure s1 = %d, s2 = %d %d\n",ci->s1, ci->s2);
	a1logd(p->log, 3, "               s3 = %d %d %d %d %d %d\n",
	       ci->s3[0], ci->s3[1], ci->s3[2], ci->s3[3], ci->s3[4], ci->s3[5]);
	a1logd(p->log, 3, "               s4 = %d %d %d %d %d %d\n",
	       ci->s4[0], ci->s4[1], ci->s4[2], ci->s4[3], ci->s4[4], ci->s4[5]);

	/* Reset the instrument to trigger an auto-zero ? */
	if ((rv = spydX2_reset(p)) != inst_ok)
		return rv;

	write_ORD16_be(send + 0, ci->s2);
	write_ORD8(send + 2, ci->s1);
	for (i = 0; i < 6; i++)
		write_ORD8(send + 3 + i, ci->s3[i]);
	for (i = 0; i < 6; i++)
		write_ORD8(send + 9 + i, ci->s4[i]);

	se = spydX2_command(p, 0xF2, send, 0xf, reply, 0xc, 0, 5.0);  

	if (se != SPYDX2_OK) {
		rv = spydX2_interp_code((inst *)p, icoms2spydX2_err(se));
		a1logd(p->log, 6, "spydX2_Measure: failed with ICOM code 0x%x\n",rv);
		return rv;
	}

	for (i = 0; i < 6; i++)
		raw[i] = read_ORD16_be(reply + 2 * i);

	a1logd(p->log, 3, "spydX2_Measure got raw = %d %d %d %d %d %d\n",
	       raw[0], raw[1], raw[2], raw[3], raw[4], raw[5]);

	return rv;
}

/* Do a ready cooked measurement from the 2024. */
static inst_code
spyd2024_GetReading(
	spydX2 *p,
	double xyz[3]				/* return XYZ */
) {
	SpX2calinfo *ci = &p->cinfo[p->ix];
	int i;
	ORD8 send[1];
	ORD8 reply[13];
	int se = ICOM_OK;
	inst_code rv = inst_ok;

	a1logd(p->log, 3, "spyd2024_GetReading\n");

	if (!p->is2024) {
		rv = spydX2_interp_code((inst *)p, SPYDX2_WRONG_INST);
		a1logd(p->log, 6, "Wrong instrument, expect 2024 and got X2\n");
		return rv;
	}

	/* Reset the instrument to trigger an auto-zero ? */
	if ((rv = spydX2_reset(p)) != inst_ok)
		return rv;

	send[0] = p->ix;

	se = spydX2_command(p, 0xFA, send, 1, reply, 13, 0, 5.0);  

	if (se != SPYDX2_OK) {
		rv = spydX2_interp_code((inst *)p, icoms2spydX2_err(se));
		a1logd(p->log, 6, "spydX2_Measure: failed with ICOM code 0x%x\n",rv);
		return rv;
	}

	if (reply[0] != p->ix) {
		rv = spydX2_interp_code((inst *)p, SPYDX2_CIX_MISMATCH);
		a1logd(p->log, 6, "spydX2_Measure: got unexpected display no. back. 0x%x\n",rv);
	}

	for (i = 0; i < 3; i++) {
		ORD32 vv = read_ORD32_le(reply + 1 + i * 4);
		xyz[i] = IEEE754todouble(vv);
	}

	a1logd(p->log, 3, "spyd2024_GetReading got XYZ = %f %f %f\n", xyz[0], xyz[1], xyz[2]);

	return rv;
}


/* Measure ambient light */
/* Gain settings: 0x00 = 1.0, 0x01 = 8.0, 0x10 = 16.0, 0x11 = 120.0 */
static inst_code
spydX2_AmbMeasure(
	spydX2 *p,
	int raw[4],			/* Return four ambient values (last two same as ap[] ?) */
	int ap[2]			/* Parameters, integration time, gain control bits */
) {
	int i;
	ORD8 send[0x2];
	ORD8 reply[0x6];
	int se = ICOM_OK;
	inst_code rv = inst_ok;

	a1logd(p->log, 3, "spydX2_AmbMeasure av = %d, %d\n", ap[0], ap[1]);

	write_ORD8(send + 0, ap[0]);
	write_ORD8(send + 1, ap[1]);

	se = spydX2_command(p, 0xD4, send, 0x2, reply, 0x6, 0, 5.0);  

	if (se != SPYDX2_OK) {
		rv = spydX2_interp_code((inst *)p, icoms2spydX2_err(se));
		a1logd(p->log, 6, "spydX2_AmbMeasure: failed with ICOM code 0x%x\n",rv);
		return rv;
	}

	raw[0] = read_ORD16_be(reply + 2 * 0);
	raw[1] = read_ORD16_be(reply + 2 * 1);
	raw[2] = read_ORD8(reply + 4);				/* Returns ap[0] */
	raw[3] = read_ORD8(reply + 5);				/* Returns ap[1] */

	a1logd(p->log, 3, "spydX2_AmbMeasure got raw %d %d\n", raw[0], raw[1]);

	return rv;
}

/* =================================================================== */
/* Medium level commands */

/* Do a reading. */
static inst_code
spydX2_GetReading(
	spydX2 *p,
	double *XYZ		/* return the XYZ values */
) {
	inst_code rv = inst_ok;
	int ix = p->ix;
	SpX2calinfo *ci = &p->cinfo[ix];
	int raw[6];
	int i;

	/* Do measurement setup. */
	rv = spydX2_measSettup(p);

	if (rv != inst_ok)
		return rv;

	/* Do measurement */
	rv = spydX2_Measure(p, raw);
	if (rv != inst_ok)
		return rv;

	/* Subtract black cal */
	for (i = 0; i < 6; i++) {
		raw[i] -= ci->s5[i] + p->bcal[i];
		if (raw[i] < 0)		// Hmm. Black cal is not so good...
			raw[i] = 0;
	}

	/* Apply calibration matrix */
	XYZ[0] = ci->mat[0][0] * (double)raw[0] 
	       + ci->mat[0][1] * (double)raw[1]
	       + ci->mat[0][2] * (double)raw[2]
	       + ci->mat[0][3] * (double)raw[3]
	       + ci->mat[0][4] * (double)raw[4]
	       + ci->mat[0][5] * (double)raw[5];

	XYZ[1] = ci->mat[1][0] * (double)raw[0] 
	       + ci->mat[1][1] * (double)raw[1]
	       + ci->mat[1][2] * (double)raw[2]
	       + ci->mat[1][3] * (double)raw[3]
	       + ci->mat[1][4] * (double)raw[4]
	       + ci->mat[1][5] * (double)raw[5];

	XYZ[2] = ci->mat[2][0] * (double)raw[0] 
	       + ci->mat[2][1] * (double)raw[1]
	       + ci->mat[2][2] * (double)raw[2]
	       + ci->mat[2][3] * (double)raw[3]
	       + ci->mat[2][4] * (double)raw[4]
	       + ci->mat[2][5] * (double)raw[5];

	/* Apply gain and offset */
	XYZ[0] = XYZ[0] * ci->gain[0] + ci->off[0];
	XYZ[1] = XYZ[1] * ci->gain[1] + ci->off[1];
	XYZ[2] = XYZ[2] * ci->gain[2] + ci->off[2];

	a1logd(p->log, 3, "spydX2_GetReading: final XYZ reading %f %f %f\n",XYZ[0], XYZ[1], XYZ[2]);

	return rv;
}

/* Do an ambient reading */
/* This appears to be identical to the SpyderX ? */

/* NOTE :- the ambient sensor seem to be an AMS TSL25721. */
/* It has two sensors, one wide band and the other infra-red, */
/* the idea being to subtract them to get a rough human response. */
/* The reading is 16 bits, with 2 bits of gain and 8 bits of integration time control */
static inst_code
spydX2_GetAmbientReading(
	spydX2 *p,
	double *XYZ		/* return the ambient XYZ values */
) {
	int ap[2], raw[4];
	double amb0, amb1, gain, intt, atten;
	double cpl, lux1, lux2, tlux, amb;
	inst_code ev = inst_ok;

	a1logd(p->log, 3, "spydX2_GetAmbientReading: called\n");

	/* Do measurement. */
	ap[0] = 101;	/* Integration time */
	ap[1] = 0x10;	/* Gain setting 16 */
	if ((ev = spydX2_AmbMeasure(p, raw, ap)) != inst_ok)
		return ev;

	amb0 = raw[0];
	amb1 = raw[1];
	
	intt = (double)raw[2];

	switch(raw[3]) {
		case 0x00:
			gain = 1.0;
			break;
		case 0x01:
			gain = 8.0;
			break;
		case 0x10:
		default:
			gain = 16.0;
			break;
		case 0x11:
			gain = 120.0;
			break;
	}

	/* Attenuation/calbration. This is very rough, */
	/* because the SpyderX ambient sensor seems to be quite directional, */
	/* as well as having a poor  spectral characteristic, */
	/* which shouldn't be the case for a true ambient sensor. */
	atten = 44.0;

	/* Counts per lux */
	cpl = (intt * gain) / (atten * 60.0);

	lux1 = (amb0 - 1.87 * amb1)/cpl;
	lux2 = (0.63 * amb0 - amb1)/cpl;
	tlux = lux1 > lux2 ? lux1 : lux2;
	tlux = tlux < 0.0 ? 0.0 : tlux;
	
//	a1logd(p->log, 4, "spydX2_GetAmbientReading: cpl %f lux1 %f lux2 %f\n",cpl, lux1, lux2);

	/* Compute the Y value */
	XYZ[1] = tlux;						/*  */
	XYZ[0] = icmD50.X * XYZ[1];			/* Convert to D50 neutral */
	XYZ[2] = icmD50.Z * XYZ[1];

	a1logd(p->log, 3, "spydX2_GetAmbientReading: returning %f %f %f\n",XYZ[0],XYZ[1],XYZ[2]);

	return ev;
}

/* This probably isn't entirely right - we really need to calibrate */
/* black for each display type, because they can change sensor gains. */
/* (Could fudge it by scaling "High Brightness" offsets bt 0.5...) */ 
static inst_code
spydX2_BlackCal(
	spydX2 *p
) {
	inst_code rv = inst_ok;
	int ix = p->ix;
	SpX2calinfo *ci = &p->cinfo[ix];
	int raw[6];
	int i;

	/* Do measurement setup. */
	rv = spydX2_measSettup(p);
	if (rv != inst_ok)
		return rv;

	/* Do measurement */
	rv = spydX2_Measure(p, raw);
	if (rv != inst_ok)
		return rv;

	/* New calibration values */
	for (i = 0; i < 6; i++) 
		p->bcal[i] = raw[i] - ci->s5[i];

	a1logd(p->log, 3, "spydX2_BlackCal: offsets %d %d %d %d %d %d\n",
		p->bcal[0], p->bcal[1], p->bcal[2], p->bcal[3], p->bcal[4], p->bcal[5]);

	return rv;
}


/* ============================================================ */

/* Establish communications with a SPYDX2 */
/* If it's a serial port, use the baud rate given, and timeout in to secs */
/* Return SPYDX2_COMS_FAIL on failure to establish communications */
static inst_code
spydX2_init_coms(inst *pp, baud_rate br, flow_control fc, double tout) {
	spydX2 *p = (spydX2 *) pp;
	int se;
	icomuflags usbflags = icomuf_none;

	a1logd(p->log, 2, "spydX2_init_coms: about to init coms\n");

	if (p->icom->port_type(p->icom) != icomt_usb) {
		a1logd(p->log, 1, "spydX2_init_coms: wrong communications type for device!\n");
		return inst_coms_fail;
	}

	a1logd(p->log, 2, "spydX2_init_coms: about to init USB\n");

#if defined(UNIX_X11)
    /* On Linux, it doesn't seem to close properly, and won't re-open */
    usbflags |= icomuf_detach;
    usbflags |= icomuf_reset_before_close;
#endif

#if defined(NT) || defined(UNIX_APPLE)
	/* On MSWin it doesn't like clearing on open when running direct (i.e not HID) */
	usbflags |= icomuf_no_open_clear;
#endif

	/* Set config, interface, write end point, read end point */
	/* ("serial" end points aren't used - the spydX2 uses USB control & write/read) */
	if ((se = p->icom->set_usb_port(p->icom, 1, 0x00, 0x00, usbflags, 0, NULL)) != ICOM_OK) { 
		a1logd(p->log, 1, "spydX2_init_coms: failed ICOM err 0x%x\n",se);
		return spydX2_interp_code((inst *)p, icoms2spydX2_err(se));
	}

	a1logd(p->log, 2, "spydX2_init_coms: succeeded\n");

	p->gotcoms = 1;
	return inst_ok;
}

static inst_code set_default_disp_type(spydX2 *p);

/* Initialise the SPYDX2 */
/* return non-zero on an error, with an inst_code */
static inst_code
spydX2_init_inst(inst *pp) {
	spydX2 *p = (spydX2 *)pp;
	inst_code ev = inst_ok;
	int stat;
	int i;

	a1logd(p->log, 2, "spydX2_init_inst: called\n");

	if (p->gotcoms == 0) /* Must establish coms before calling init */
		return spydX2_interp_code((inst *)p, SPYDX2_NO_COMS);

	if (p->dtype != instSpyderX2
	 && p->dtype != instSpyder2024)
		return spydX2_interp_code((inst *)p, SPYDX2_UNKNOWN_MODEL);

	/* Reset the instrument */
	if ((ev = spydX2_reset(p)) != inst_ok)
		return ev;

	/* Get HW version and serial number */
	if ((ev = spydX2_getInstInfo(p)) != inst_ok)
		return ev;

	/* Set a default calibration */
	if ((ev = set_default_disp_type(p)) != inst_ok) {
		return ev;
	}

	p->lo_secs = 2000000000;			/* A very long time */

#ifdef ENABLE_NONVCAL
	/* Restore the all modes calibration from the local system */
	spydX2_restore_calibration(p);

	/* Touch it so that we know when the instrument was last opened */
	spydX2_touch_calibration(p);
#endif

	/* Do an ambient measurement to initialize it */
	{
		int ap[2] = { 101, 0x10 }, raw[4];
		spydX2_AmbMeasure(p, raw, ap);
	}

	p->trig = inst_opt_trig_user;		/* default trigger mode */

	p->inited = 1;
	a1logd(p->log, 2, "spydX2_init_inst: inited OK\n");

	a1logv(p->log, 1, "Instrument Type:   %s\n"
		              "Serial Number:     %s\n"
		              "Hardware version:  %d.%02d\n"
	                  ,inst_name(p->dtype) ,p->serno ,p->hwvn[0],p->hwvn[1]);


	return inst_ok;
}

/* Read a single sample */
/* Return the dtp error code */
static inst_code
spydX2_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val,		 /* Pointer to instrument patch value */
instClamping clamp) {		/* NZ if clamp XYZ/Lab to be +ve */
	spydX2 *p = (spydX2 *)pp;
	int user_trig = 0;
	inst_code ev = inst_protocol_error;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->trig == inst_opt_trig_user) {

		if (p->uicallback == NULL) {
			a1logd(p->log, 1, "spydX2: inst_opt_trig_user but no uicallback function set!\n");
			return inst_unsupported;
		}

		for (;;) {
			if ((ev = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
				if (ev == inst_user_abort)
					return ev;				/* Abort */
				if (ev == inst_user_trig) {
					user_trig = 1;
					break;					/* Trigger */
				}
			}
			msec_sleep(200);
		}
		/* Notify of trigger */
		if (p->uicallback)
			p->uicallback(p->uic_cntx, inst_triggered); 

	/* Progromatic Trigger */
	} else {
		/* Check for abort */
		if (p->uicallback != NULL
		 && (ev = p->uicallback(p->uic_cntx, inst_armed)) == inst_user_abort) {
			return ev;				/* Abort */
		}
	}

	if (IMODETST(p->mode, inst_mode_emis_ambient)) {
		ev = spydX2_GetAmbientReading(p, val->XYZ);

	} else {
		ev = inst_ok;

		/* Read the XYZ value */
		if (p->is2024 && p->usehl)
			ev = spyd2024_GetReading(p, val->XYZ);		/* High level command */
		else
			ev = spydX2_GetReading(p, val->XYZ);		/* Low level commands */

		if (ev == inst_ok) {

			/* Apply the colorimeter correction matrix */
			icmMulBy3x3(val->XYZ, p->ccmat, val->XYZ);
		}
	}

	if (ev != inst_ok)
		return ev;


	/* This may not change anything since instrument may clamp */
	if (clamp)
		icmClamp3(val->XYZ, val->XYZ);

	val->loc[0] = '\000';
	if (IMODETST(p->mode, inst_mode_emis_ambient))
		val->mtype = inst_mrt_ambient;
	else
		val->mtype = inst_mrt_emission;
	val->mcond = inst_mrc_none;
	val->XYZ_v = 1;		/* These are absolute XYZ readings ? */
	val->sp.spec_n = 0;
	val->duration = 0.0;


	if (user_trig)
		return inst_user_trig;
	return ev;
}

static inst_code set_base_disp_type(spydX2 *p, int cbid);

/* Insert a colorimetric correction matrix in the instrument XYZ readings */
/* This is only valid for colorimetric instruments. */
/* To remove the matrix, pass NULL for the filter filename */
inst_code spydX2_col_cor_mat(
inst *pp,
disptech dtech,		/* Use disptech_unknown if not known */				\
int cbid,       	/* Calibration display type base ID, 1 if unknown */\
double mtx[3][3]
) {
	spydX2 *p = (spydX2 *)pp;
	inst_code ev = inst_ok;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if ((ev = set_base_disp_type(p, cbid)) != inst_ok)
		return ev;
	if (mtx == NULL)
		icmSetUnity3x3(p->ccmat);
	else
		icmCpy3x3(p->ccmat, mtx);

	p->dtech = dtech;
	p->cbid = 0;	/* Can't be base type now */

	if (p->log->debug >= 4) {
		a1logd(p->log,4,"ccmat           = %f %f %f\n",
		                 p->ccmat[0][0], p->ccmat[0][1], p->ccmat[0][2]);
		a1logd(p->log,4,"                  %f %f %f\n",
		                 p->ccmat[1][0], p->ccmat[1][1], p->ccmat[1][2]);
		a1logd(p->log,4,"                  %f %f %f\n\n",
		                 p->ccmat[2][0], p->ccmat[2][1], p->ccmat[2][2]);
		a1logd(p->log,4,"ucbid = %d, cbid = %d\n",p->ucbid, p->cbid);
		a1logd(p->log,4,"\n");
	}

	return ev;
}

/* Return needed and available inst_cal_type's */
static inst_code spydX2_get_n_a_cals(inst *pp, inst_cal_type *pn_cals, inst_cal_type *pa_cals) {
	spydX2 *p = (spydX2 *)pp;
	time_t curtime = time(NULL);
	inst_cal_type n_cals = inst_calt_none;
	inst_cal_type a_cals = inst_calt_none;
	
	if ((curtime - p->bdate) > DCALTOUT) {
		a1logd(p->log,2,"SpydX: Invalidating black cal as %d secs from last cal\n",curtime - p->bdate);
		p->bcal_done = 0;
	}
		
	if (!IMODETST(p->mode, inst_mode_emis_ambient)) {		/* If not ambient */
		if (!p->is2024 || !p->usehl) {
#ifdef ENABLE_BLACK_CAL
			if (!p->bcal_done || !p->noinitcalib)
				n_cals |= inst_calt_emis_offset;
#endif	/* ENABLE_BLACK_CAL */
			a_cals |= inst_calt_emis_offset;
		}
	}

	a1logd(p->log,4,"SpydX: returning n_cals 0x%x, a_cals 0x%x\n",n_cals,a_cals);

	if (pn_cals != NULL)
		*pn_cals = n_cals;

	if (pa_cals != NULL)
		*pa_cals = a_cals;

	return inst_ok;
}

/* Request an instrument calibration. */
inst_code spydX2_calibrate(
inst *pp,
inst_cal_type *calt,	/* Calibration type to do/remaining */
inst_cal_cond *calc,	/* Current condition/desired condition */
inst_calc_id_type *idtype,	/* Condition identifier type */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	spydX2 *p = (spydX2 *)pp;
    inst_cal_type needed, available;
	inst_code ev;
	int ec;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if ((ev = spydX2_get_n_a_cals((inst *)p, &needed, &available)) != inst_ok)
		return ev;

	/* Translate inst_calt_all/needed into something specific */
	if (*calt == inst_calt_all
	 || *calt == inst_calt_needed
	 || *calt == inst_calt_available) {
		if (*calt == inst_calt_all) 
			*calt = (needed & inst_calt_n_dfrble_mask) | inst_calt_ap_flag;
		else if (*calt == inst_calt_needed)
			*calt = needed & inst_calt_n_dfrble_mask;
		else if (*calt == inst_calt_available)
			*calt = available & inst_calt_n_dfrble_mask;

		a1logd(p->log,4,"spydX2_calibrate: doing calt 0x%x\n",calt);

		if ((*calt & inst_calt_n_dfrble_mask) == 0)		/* Nothing todo */
			return inst_ok;
	}

	/* See if it's a calibration we understand */
	if (*calt & ~available & inst_calt_all_mask) { 
		return inst_unsupported;
	}

	/* Black calibration: */
	if (!p->is2024 || !p->usehl) {
		if (*calt & inst_calt_emis_offset) {
			time_t cdate = time(NULL);
	
			if ((*calc & inst_calc_cond_mask) != inst_calc_man_em_dark) {
				*calc = inst_calc_man_em_dark;
				return inst_cal_setup;
			}
	
			/* Do black offset calibration */
			if ((ev = spydX2_BlackCal(p)) != inst_ok)
				return ev;
			p->bcal_done = 1;
			p->bdate = cdate;
			p->noinitcalib = 1;			/* Don't calibrate again */
		}
	}

#ifdef ENABLE_NONVCAL
	/* Save the calibration to a file */
	spydX2_save_calibration(p);
#endif

	return inst_ok;
}

/* Error codes interpretation */
static char *
spydX2_interp_error(inst *pp, int ec) {
//	spydX2 *p = (spydX2 *)pp;
	ec &= inst_imask;
	switch (ec) {
		case SPYDX2_INTERNAL_ERROR:
			return "Non-specific software internal software error";
		case SPYDX2_COMS_FAIL:
			return "Communications failure";
		case SPYDX2_UNKNOWN_MODEL:
			return "Not a Spyder X2";
		case SPYDX2_DATA_PARSE_ERROR:
			return "Data from i1 Display didn't parse as expected";
		case SPYDX2_INT_CAL_SAVE:
			return "Saving calibration file failed";
		case SPYDX2_INT_CAL_RESTORE:
			return "Restoring calibration file failed";
		case SPYDX2_INT_CAL_TOUCH:
			return "Touching calibration file failed";

		case SPYDX2_OK:
			return "No device error";

		/* device specific errors */
		default:
			return "Unknown error code";
	}
}


/* Convert a machine specific error code into an abstract dtp code */
static inst_code 
spydX2_interp_code(inst *pp, int ec) {
//	spydX2 *p = (spydX2 *)pp;

	ec &= inst_imask;
	switch (ec) {

		case SPYDX2_OK:
			return inst_ok;


		case SPYDX2_INTERNAL_ERROR:
			return inst_internal_error | ec;

		case SPYDX2_COMS_FAIL:
		case SPYDX2_DATA_PARSE_ERROR:
			return inst_coms_fail | ec;

		case SPYDX2_UNKNOWN_MODEL:
			return inst_unknown_model | ec;

		case SPYDX2_CIX_MISMATCH:
			return inst_wrong_setup | ec;

		case SPYDX2_WRONG_INST:
			return inst_internal_error | ec;
		
//			return inst_protocol_error | ec;
//			return inst_hardware_fail | ec;
//			return inst_misread | ec;

	}
	return inst_other_error | ec;
}

/* Destroy ourselves */
static void
spydX2_del(inst *pp) {
	spydX2 *p = (spydX2 *)pp;

#ifdef ENABLE_NONVCAL
	if (p->inited) {
		/* Touch it so that we know when the instrument was last open */
		spydX2_touch_calibration(p);
	}
#endif

	if (p->icom != NULL)
		p->icom->del(p->icom);
	p->vdel(pp);
	free(p);
}

/* Set the noinitcalib mode */
static void spydX2_set_noinitcalib(spydX2 *p, int v, int losecs) {

	/* Ignore disabling init calib if more than losecs since instrument was open */
	if (v && losecs != 0 && p->lo_secs >= losecs) {
		a1logd(p->log,3,"initcalib disable ignored because %d >= %d secs\n",p->lo_secs,losecs);
		return;
	}
	p->noinitcalib = v;
}

/* Return the instrument mode capabilities */
static void spydX2_capabilities(inst *pp,
inst_mode *pcap1,
inst2_capability *pcap2,
inst3_capability *pcap3) {
	spydX2 *p = (spydX2 *)pp;
	inst_mode cap1 = 0;
	inst2_capability cap2 = 0;

	cap1 |= inst_mode_emis_spot
	     |  inst_mode_colorimeter
	     |  inst_mode_emis_ambient
	        ;


	cap2 |= inst2_prog_trig
	     |  inst2_user_trig
	     |  inst2_ccmx
		 |  inst2_disptype
		 |  inst2_ambient_mono
	        ;

	if (pcap1 != NULL)
		*pcap1 = cap1;
	if (pcap2 != NULL)
		*pcap2 = cap2;
	if (pcap3 != NULL)
		*pcap3 = inst3_none;
}

/* Check device measurement mode */
static inst_code spydX2_check_mode(inst *pp, inst_mode m) {
	spydX2 *p = (spydX2 *)pp;
	inst_mode cap;

	if (!p->gotcoms)
		return inst_no_coms;

	if (!p->inited)
		return inst_no_init;

	pp->capabilities(pp, &cap, NULL, NULL);

	/* Simple test */
	if (m & ~cap)
		return inst_unsupported;

	if (!IMODETST(m, inst_mode_emis_spot)
	 && !IMODETST(m, inst_mode_emis_ambient)) {
			return inst_unsupported;
	}

	return inst_ok;
}

/* Set device measurement mode */
static inst_code spydX2_set_mode(inst *pp, inst_mode m) {
	spydX2 *p = (spydX2 *)pp;
	inst_code ev;

	if ((ev = spydX2_check_mode(pp, m)) != inst_ok)
		return ev;

	p->mode = m;

	return inst_ok;
}

static inst_disptypesel spydX2_disptypesel[SPYDX2_NOCALIBS+1] = {
	{
		inst_dtflags_mtx | inst_dtflags_default,		/* flags */
		1,							/* cbid */
		"l",						/* sel */
		"General",					/* desc */
		0,							/* refr */
		disptech_lcd_ccfl,			/* disptype */
		0							/* ix */
	},
	{
		inst_dtflags_mtx,			/* flags */
		0,							/* cbid */
		"e",						/* sel */
		"Standard LED",				/* desc */
		1,							/* refr */
		disptech_lcd_wled,			/* disptype */
		1							/* ix */
	},
	{
		inst_dtflags_mtx,			/* flags */
		0,							/* cbid */
		"b",						/* sel */
		"Wide Gamut LED",			/* desc */
		1,							/* refr */
		disptech_lcd_rgbled,		/* disptype */
		2							/* ix */
	},
	{
		inst_dtflags_mtx,			/* flags */
		0,							/* cbid */
		"i",						/* sel */
		"GB LED",					/* desc */
		1,							/* refr */
		disptech_lcd_gbrledp,		/* disptype */
		3							/* ix */
	},
	{
		inst_dtflags_mtx,			/* flags */
		0,							/* cbid */
		"h",						/* sel */
		"High Brightness",			/* desc */
		1,							/* refr */
		disptech_lcd_wled,			/* disptype (assumed) */
		4							/* ix */
	},
	{
		inst_dtflags_end,
		0,
		"",
		"",
		0,
		disptech_none,
		0
	}
};

static inst_disptypesel spyd2024_disptypesel[SPYD2024_NOCALIBS+1] = {
	{
		inst_dtflags_mtx | inst_dtflags_default,		/* flags */
		1,							/* cbid */
		"l",						/* sel */
		"General",					/* desc */
		0,							/* refr */
		disptech_lcd_ccfl,			/* disptype */
		0							/* ix */
	},
	{
		inst_dtflags_mtx,			/* flags */
		0,							/* cbid */
		"e",						/* sel */
		"Standard LED",				/* desc */
		1,							/* refr */
		disptech_lcd_wled,			/* disptype */
		1							/* ix */
	},
	{
		inst_dtflags_mtx,			/* flags */
		0,							/* cbid */
		"b",						/* sel */
		"Wide Gamut LED",			/* desc */
		1,							/* refr */
		disptech_lcd_rgbled,		/* disptype */
		2							/* ix */
	},
	{
		inst_dtflags_mtx,			/* flags */
		0,							/* cbid */
		"i",						/* sel */
		"GB LED",					/* desc */
		1,							/* refr */
		disptech_lcd_gbrledp,		/* disptype */
		3							/* ix */
	},
	{
		inst_dtflags_mtx,			/* flags */
		0,							/* cbid */
		"h",						/* sel */
		"High Brightness",			/* desc */
		1,							/* refr */
		disptech_lcd_wled,			/* disptype (assumed) */
		4							/* ix */
	},
	{
		inst_dtflags_mtx,			/* flags */
		0,							/* cbid */
		"o",						/* sel */
		"OLED",						/* desc */
		1,							/* refr */
		disptech_oled,				/* disptype (assumed) */
		5							/* ix */
	},
	{
		inst_dtflags_mtx,			/* flags */
		0,							/* cbid */
		"m",						/* sel */
		"Mini-LED",					/* desc */
		1,							/* refr */
		disptech_lcd_rgbled,		/* disptype (assumed) */
		6							/* ix */
	},
	{
		inst_dtflags_end,
		0,
		"",
		"",
		0,
		disptech_none,
		0
	}
};


/* Get mode and option details */
static inst_code spydX2_get_disptypesel(
inst *pp,
int *pnsels,				/* Return number of display types */
inst_disptypesel **psels,	/* Return the array of display types */
int allconfig,				/* nz to return list for all configs, not just current. */
int recreate				/* nz to re-check for new ccmx & ccss files */
) {
	spydX2 *p = (spydX2 *)pp;
	inst_code rv = inst_ok;

	/* Create/Re-create a current list of available display types. */
	if (p->dtlist == NULL || recreate) {
		if (p->is2024) {
			if ((rv = inst_creat_disptype_list(pp, &p->ndtlist, &p->dtlist,
			    spyd2024_disptypesel, 0 /* doccss */, 1 /* doccmx */)) != inst_ok)
				return rv;
		} else {
			if ((rv = inst_creat_disptype_list(pp, &p->ndtlist, &p->dtlist,
			    spydX2_disptypesel, 0 /* doccss */, 1 /* doccmx */)) != inst_ok)
				return rv;
		}
	}

	if (pnsels != NULL)
		*pnsels = p->ndtlist;

	if (psels != NULL)
		*psels = p->dtlist;

	return inst_ok;
}

/* Given a display type entry, setup for that type */
static inst_code set_disp_type(spydX2 *p, inst_disptypesel *dentry) {

	/* If an inbuilt matrix hasn't been read from the instrument, */
	/* read it now. */
	if ((dentry->flags & inst_dtflags_mtx) 
	 && (dentry->flags & inst_dtflags_ld) == 0) { 
		inst_code rv;
		int ix = dentry->ix; 

		p->ix = ix;
		p->cinfo[ix].ix = ix;

		/* See if we could/should use high level measure XYZ for Spyder 2024 */
		p->usehl = 0;
		if ( p->is2024
		 && !p->forcell
		 && ix < p->mxdnp1
   		 && ((1 << ix) & p->dnomask) != 0) {	
			p->usehl = 1;
		}

		if (!p->is2024 || !p->usehl) {
			rv = spydX2_getCalibration(p);
	
			if (rv != inst_ok)
				return rv;
		}

		icmSetUnity3x3(dentry->mat);			/* Not used for native calibration */

		dentry->flags |= inst_dtflags_ld;		/* It's now loaded */
	}

	if (dentry->flags & inst_dtflags_ccmx) {
		if (dentry->cc_cbid != 1) {
			a1loge(p->log, 1, "SpydX: matrix must use cbid 1 (is %d)!\n",dentry->cc_cbid);
			return inst_wrong_setup;
		}

		p->dtech = dentry->dtech;
		icmCpy3x3(p->ccmat, dentry->mat);
		p->cbid = 0;	/* Can't be a base type now */

	} else if ((dentry->flags & inst_dtflags_mtx) != 0) { 
		p->dtech = dentry->dtech;
		icmCpy3x3(p->ccmat, dentry->mat);
		p->cbid = dentry->cbid;
		p->ucbid = dentry->cbid;    /* This is underying base if dentry is base selection */

	} else {			/* This shouldn't happen... */
		a1loge(p->log, 1, "SpydX: calibration selected isn't builit in or CCMX!\n");
		return inst_wrong_setup;
	}

	p->ix = dentry->ix;				/* Native index */

	if (p->log->debug >= 4) {
		a1logd(p->log,4,"ccmat           = %f %f %f\n",
		                 p->ccmat[0][0], p->ccmat[0][1], p->ccmat[0][2]);
		a1logd(p->log,4,"                  %f %f %f\n",
		                 p->ccmat[1][0], p->ccmat[1][1], p->ccmat[1][2]);
		a1logd(p->log,4,"                  %f %f %f\n\n",
		                 p->ccmat[2][0], p->ccmat[2][1], p->ccmat[2][2]);
		a1logd(p->log,4,"ucbid = %d, cbid = %d\n",p->ucbid, p->cbid);
		a1logd(p->log,4,"usehl = %d\n",p->usehl);
		a1logd(p->log,4,"\n");
	}

	return inst_ok;
}


/* Set the display type */
static inst_code spydX2_set_disptype(inst *pp, int ix) {
	spydX2 *p = (spydX2 *)pp;
	inst_code ev;
	inst_disptypesel *dentry;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (ix < 0 || ix >= p->ndtlist)
		return inst_unsupported;

	dentry = &p->dtlist[ix];

	if ((ev = set_disp_type(p, dentry)) != inst_ok) {
		return ev;
	}

	return inst_ok;
}

/* Setup the default display type */
static inst_code set_default_disp_type(spydX2 *p) {
	inst_code ev;
	int i;

	if (p->dtlist == NULL) {
		if (p->is2024) {
			// Hmm. Should we really use the dnomask to edit the list
			// down to what the instrument supports ??
			if ((ev = inst_creat_disptype_list((inst *)p, &p->ndtlist, &p->dtlist,
			    spyd2024_disptypesel, 0 /* doccss */, 1 /* doccmx */)) != inst_ok)
				return ev;
		} else {
			if ((ev = inst_creat_disptype_list((inst *)p, &p->ndtlist, &p->dtlist,
			    spydX2_disptypesel, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
				return ev;
			}
	}
	/* Locate the default */
	for (i = 0; !(p->dtlist[i].flags & inst_dtflags_end); i++) {
		if (p->dtlist[i].flags & inst_dtflags_default)
			break;
	}
	if (p->dtlist[i].flags & inst_dtflags_end) {
		a1loge(p->log, 1, "set_default_disp_type: failed to find type!\n");
		return inst_internal_error; 
	}
	if ((ev = set_disp_type(p, &p->dtlist[i])) != inst_ok) {
		return ev;
	}

	return inst_ok;
}

/* Setup the display type to the given base type */
static inst_code set_base_disp_type(spydX2 *p, int cbid) {
	inst_code ev;
	int i;

	if (cbid == 0) {
		a1loge(p->log, 1, "spydX2 set_base_disp_type: can't set base display type of 0\n");
		return inst_wrong_setup;
	}

	if (p->dtlist == NULL) {
		if (p->is2024) {
			if ((ev = inst_creat_disptype_list((inst *)p, &p->ndtlist, &p->dtlist,
			    spyd2024_disptypesel, 0 /* doccss */, 1 /* doccmx */)) != inst_ok)
				return ev;
		} else {
			if ((ev = inst_creat_disptype_list((inst *)p, &p->ndtlist, &p->dtlist,
			    spydX2_disptypesel, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
				return ev;
		}
	}

	for (i = 0; !(p->dtlist[i].flags & inst_dtflags_end); i++) {
		if (!(p->dtlist[i].flags & inst_dtflags_ccmx)		/* Prevent infinite recursion */
		 && p->dtlist[i].cbid == cbid)
			break;
	}
	if (p->dtlist[i].flags & inst_dtflags_end) {
		a1loge(p->log, 1, "set_base_disp_type: failed to find cbid %d!\n",cbid);
		return inst_wrong_setup; 
	}
	if ((ev = set_disp_type(p, &p->dtlist[i])) != inst_ok) {
		return ev;
	}

	return inst_ok;
}

/* Get the disptech and other corresponding info for the current */
/* selected display type. Returns disptype_unknown by default. */
static inst_code spydX2_get_disptechi(
inst *pp,
disptech *dtech,
int *refrmode,
int *cbid) {
	spydX2 *p = (spydX2 *)pp;
	if (dtech != NULL)
		*dtech = p->dtech;
	if (cbid != NULL)
		*cbid = p->cbid;
	return inst_ok;
}

/* 
 * set or reset an optional mode
 *
 * Some options talk to the instrument, and these will
 * error if it hasn't been initialised.
 * [We could fix this by setting a flag and adding
 *  some extra logic in init()]
 */
static inst_code
spydX2_get_set_opt(inst *pp, inst_opt_type m, ...) {
	spydX2 *p = (spydX2 *)pp;
	inst_code ev = inst_ok;

	if (m == inst_opt_initcalib) {			/* default */
		spydX2_set_noinitcalib(p, 0, 0);
		return inst_ok;

	} else if (m == inst_opt_noinitcalib) {		/* Disable initial calibration */
		va_list args;
		int losecs = 0;

		va_start(args, m);
		losecs = va_arg(args, int);
		va_end(args);

		spydX2_set_noinitcalib(p, 1, losecs);
		return inst_ok;
	}

	/* Record the trigger mode */
	if (m == inst_opt_trig_prog
	 || m == inst_opt_trig_user) {
		p->trig = m;
		return inst_ok;
	}

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* Use default implementation of other inst_opt_type's */
	{
		inst_code rv;
		va_list args;

		va_start(args, m);
		rv = inst_get_set_opt_def(pp, m, args);
		va_end(args);

		return rv;
	}
}

/* Constructor */
extern spydX2 *new_spydX2(icoms *icom, instType dtype) {
	spydX2 *p;

	if ((p = (spydX2 *)calloc(sizeof(spydX2),1)) == NULL) {
		a1loge(icom->log, 1, "new_spydX2: malloc failed!\n");
		return NULL;
	}

	if (dtype == instSpyder2024) {
		p->is2024 = 1;

		if (getenv("SPYD2024_LOWLEV_MEASURE") != NULL)
			p->forcell = 1;
	}


	p->log = new_a1log_d(icom->log);

	p->init_coms         = spydX2_init_coms;
	p->init_inst         = spydX2_init_inst;
	p->capabilities      = spydX2_capabilities;
	p->check_mode        = spydX2_check_mode;
	p->set_mode          = spydX2_set_mode;
	p->get_disptypesel   = spydX2_get_disptypesel;
	p->set_disptype      = spydX2_set_disptype;
	p->get_disptechi     = spydX2_get_disptechi;
	p->get_set_opt       = spydX2_get_set_opt;
	p->read_sample       = spydX2_read_sample;
	p->get_n_a_cals      = spydX2_get_n_a_cals;
	p->calibrate		 = spydX2_calibrate;
	p->col_cor_mat       = spydX2_col_cor_mat;
	p->interp_error      = spydX2_interp_error;
	p->del               = spydX2_del;

	p->icom = icom;
	p->dtype = dtype;
	p->dtech = disptech_unknown;

	return p;
}


/* =============================================================================== */
/* Calibration info save/restore to file */

static int spydX2_save_calibration(spydX2 *p) {
	int ev = SPYDX2_OK;
	int i;
	char fname[100];		/* Name */
	calf x;
	int argyllversion = ARGYLL_VERSION;
	int ss;

	snprintf(fname, 99, ".spydX2_%s.cal", p->serno);

	if (calf_open(&x, p->log, fname, 1)) {
		x.ef = 2;
		goto done;
	}

	ss = sizeof(spydX2);

	/* Some file identification */
	calf_wints(&x, &argyllversion, 1);
	calf_wints(&x, &ss, 1);
	calf_wstrz(&x, p->serno);

	/* Save the black calibration if it's valid */
	calf_wints(&x, &p->bcal_done, 1);
	calf_wtime_ts(&x, &p->bdate, 1);
	calf_wints(&x, p->bcal, 3);

	a1logd(p->log,3,"nbytes = %d, Checksum = 0x%x\n",x.nbytes,x.chsum);
	calf_wints(&x, (int *)(&x.chsum), 1);

	if (calf_done(&x))
		x.ef = 3;

  done:;
	if (x.ef != 0) {
		a1logd(p->log,2,"Writing calibration file failed with %d\n",x.ef);
		ev = SPYDX2_INT_CAL_SAVE;
	} else {
		a1logd(p->log,2,"Writing calibration file succeeded\n");
	}

	return ev;
}

/* Restore the black calibration from the local system */
static int spydX2_restore_calibration(spydX2 *p) {
	int ev = SPYDX2_OK;
	int i, j;
	char fname[100];		/* Name */
	calf x;
	int argyllversion;
	int ss, nbytes, chsum1, chsum2;
	char *serno = NULL;

	snprintf(fname, 99, ".spydX2_%s.cal", p->serno);

	if (calf_open(&x, p->log, fname, 0)) {
		x.ef = 2;
		goto done;
	}

	/* Last modified time */
	p->lo_secs = x.lo_secs;

	/* Do a dumy read to check the checksum, then a real read */
	for (x.rd = 0; x.rd < 2; x.rd++) {
		calf_rewind(&x);

		/* Check the file identification */
		calf_rints2(&x, &argyllversion, 1);
		calf_rints2(&x, &ss, 1);
		calf_rstrz2(&x, &serno);

		if (x.ef != 0
		 || argyllversion != ARGYLL_VERSION
		 || ss != (sizeof(spydX2))
		 || strcmp(serno, p->serno) != 0) {
			a1logd(p->log,2,"Identification didn't verify\n");
			if (x.ef == 0)
				x.ef = 4;
			goto done;
		}

		/* Read the black calibration if it's valid */
		calf_rints(&x, &p->bcal_done, 1);
		calf_rtime_ts(&x, &p->bdate, 1);
		calf_rints(&x, p->bcal, 3);

		/* Check the checksum */
		chsum1 = x.chsum;
		nbytes = x.nbytes;
		calf_rints2(&x, &chsum2, 1);
	
		if (x.ef != 0
		 || chsum1 != chsum2) {
			a1logd(p->log,2,"Checksum didn't verify, bytes %d, got 0x%x, expected 0x%x\n",nbytes,chsum1, chsum2);
			if (x.ef == 0)
				x.ef = 5;
			goto done;
		}
	}

	a1logd(p->log, 3, "Restored spydX2_BlackCal: offsets %d %d %d\n",p->bcal[0], p->bcal[1], p->bcal[2]);

	a1logd(p->log,5,"spydX2_restore_calibration done\n");
 done:;

	free(serno);
	if (calf_done(&x))
		x.ef = 3;

	if (x.ef != 0) {
		a1logd(p->log,2,"Reading calibration file failed with %d\n",x.ef);
		ev = SPYDX2_INT_CAL_RESTORE;
	}

	return ev;
}

static int spydX2_touch_calibration(spydX2 *p) {
	int ev = SPYDX2_OK;
	char fname[100];		/* Name */
	int rv;

	snprintf(fname, 99, ".spydX2_%s.cal", p->serno);

	if (calf_touch(p->log, fname)) {
		a1logd(p->log,2,"Touching calibration file time failed with\n");
		return SPYDX2_INT_CAL_TOUCH;
	}

	return SPYDX2_OK;
}

