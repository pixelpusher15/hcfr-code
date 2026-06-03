
/* General USB I/O support, MSWin native libusb0.sys implementation. */
/* This file is conditionaly #included into usbio.c */
 
/* 
 * Argyll Color Management System
 *
 * Author: Graeme W. Gill
 * Date:   2006/22/4
 *
 * Copyright 2006 - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include "driver_api.h"

#undef DEBUG		/* Turn on debug messages */

#define LIBUSBW1_MAX_DEVICES 255
#define LIBUSBW1_PATH_MAX 512
#define LIBUSBW1_DEFAULT_TIMEOUT 5000

#ifndef PATH_MAX
# define PATH_MAX 4096
#endif

static int icoms_usb_control_msg_w0(icoms *p, int *transferred, int requesttype, int request,
                          int value, int index, unsigned char *bytes, int size, int timeout);

/* Do a synchronous request. Return an ICOM error */
static int do_sync_io(
HANDLE handle,
unsigned int ioctl,
void *out, int outsz,
void *in, int insz,
int *retsz) {
	OVERLAPPED olaps;
	DWORD xlength;

	memset(&olaps, 0, sizeof(OVERLAPPED));	
	if (retsz != NULL)
		*retsz = 0;

	if ((olaps.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL)
		return ICOM_SYS;
	
	if (!DeviceIoControl(handle, ioctl, out, outsz, in, insz, &xlength, &olaps)) {
		if (GetLastError() != ERROR_IO_PENDING) {
			CloseHandle(olaps.hEvent);
			return ICOM_USBW;
		}
		if (!GetOverlappedResult(handle, &olaps, &xlength, TRUE)) {
			CloseHandle(olaps.hEvent);
			return ICOM_USBR;
		}
	}
	CloseHandle(olaps.hEvent);
	if (retsz != NULL)
		*retsz = (int)xlength;

	return ICOM_OK;
}

/* Add paths to USB connected device */
/* Return an icom error */
int usb_get_paths_w0(
icompaths *p 
) {
	unsigned int vid, pid, nep10 = 0xffff;
	unsigned int configix, nconfig, nifce;
	devType itype;
	struct usb_idevice *usbd = NULL;
	int rv, retsz, i;
	int upto;

	upto = p->get_cur_count(p);

	for (i = 0; i < LIBUSBW1_MAX_DEVICES; i++) {
		libusb_request req;
		char dpath[LIBUSBW1_PATH_MAX];
		HANDLE handle;
		unsigned char buf[IUSB_DESC_TYPE_STRING_MAX_SIZE];

		_snprintf(dpath, LIBUSBW1_PATH_MAX - 1,"\\\\.\\libusb0-%04d", i+1);
		if (i < 16) 	/* Suppress messages on unlikely ports */
			a1logd(p->log, 6, "usb_get_paths_w0 opening device '%s'\n",dpath);

		if ((handle = CreateFile(dpath, 0, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED,
		                                                    NULL)) == INVALID_HANDLE_VALUE) {
#ifdef DEBUG
			a1logd(p->log, 8, "usb_get_paths_w0 failed to open device '%s'\n",dpath);
#endif
			continue;
		}

		/* Set kernel message debug */
		if (p->log->debug >= 6) {
			req.debug.level = LIBUSB_DEBUG_MAX;
			req.timeout = LIBUSBW1_DEFAULT_TIMEOUT;
			if (do_sync_io(handle, LIBUSB_IOCTL_SET_DEBUG_LEVEL,
				&req, sizeof(libusb_request), NULL, 0, NULL)) {
				a1logd(p->log, 1, "usb_get_paths_w0 failed to set driver log leve\n");
			} else {
				a1logd(p->log, 1, "usb_get_paths_w0 turned on kernel debug messages\n");
			}
		}

		/* Read the device descriptor */
		req.descriptor.type        = IUSB_DESC_TYPE_DEVICE;
		req.descriptor.recipient   = IUSB_REQ_RECIP_DEVICE;
		req.descriptor.index       = 0;
		req.descriptor.language_id = 0;
		req.timeout                = LIBUSBW1_DEFAULT_TIMEOUT;
		
		if (do_sync_io(handle, LIBUSB_IOCTL_GET_DESCRIPTOR, 
			                &req, sizeof(libusb_request), 
			                buf, IUSB_DESC_TYPE_DEVICE_SIZE, &retsz) != ICOM_OK
			                                     || retsz != IUSB_DESC_TYPE_DEVICE_SIZE) {
			a1logd(p->log, 1, "usb_get_paths_w0 failed to read device descriptor '%s'\n",dpath);
			CloseHandle(handle);
			continue;
		}

		/* Extract the vid and pid */ 	
		vid = usb2ushort(buf + 8);
		pid = usb2ushort(buf + 10);
		nconfig = buf[17];
		/*
			Indexes of DESCRIPTOR strings:
			iManufacturer	buf[14]
			iProduct		buf[15]
			iSerialNumber	buf[16]
		*/
		
		a1logd(p->log, 6, "usb_get_paths_w0 checking vid 0x%04x, pid 0x%04x\n",vid,pid);

		/* Do a preliminary match */
		if ((itype = inst_usb_match(vid, pid, 0)) == instUnknown) {
			a1logd(p->log, 6 , "usb_get_paths_w0 instrument not reconized\n");
			CloseHandle(handle);
			continue;
		}

		/* Allocate an idevice so that we can fill in the end point information */
		if ((usbd = (struct usb_idevice *) calloc(sizeof(struct usb_idevice), 1)) == NULL) {
			a1loge(p->log, ICOM_SYS, "usb_get_paths_w0 calloc failed!\n");
			CloseHandle(handle);
			return ICOM_SYS;
		}

		usbd->iserialno = buf[16];
		usbd->nconfig = nconfig;
		
		/* Read the configuration descriptors looking for the first configuration, first interface, */
		/* and extract the number of end points for each configuration */
		for (configix = 0; configix < nconfig; configix++) {
			int configno, totlen;
			unsigned char *buf2, *bp, *zp;
			unsigned int ninfaces, inface, nep;

			/* Read the configuration descriptor */
			req.descriptor.type        = IUSB_DESC_TYPE_CONFIG;
			req.descriptor.recipient   = IUSB_REQ_RECIP_DEVICE;
			req.descriptor.index       = configix;
			req.descriptor.language_id = 0;
			req.timeout                = LIBUSBW1_DEFAULT_TIMEOUT;
			
			/* Read just the first part of the config descriptor to get its size and no. */
			if ((rv = do_sync_io(handle, LIBUSB_IOCTL_GET_DESCRIPTOR, 
				                &req, sizeof(libusb_request), 
				                buf, IUSB_DESC_TYPE_CONFIG_SIZE, &retsz)) != ICOM_OK
				                                     || retsz != IUSB_DESC_TYPE_CONFIG_SIZE) {
				a1logd(p->log, 1, "usb_get_paths_w0 failed to read configix  %d descriptor\n",configix);
				free(usbd);
				CloseHandle(handle);
				break;
			}
			nifce = buf[4];      	/* number of interfaces */
			configno = buf[5];		/* Configuration number */

			if (configno != 1)
				continue;

			if ((totlen = usb2ushort(buf + 2)) < 6) {
				a1logd(p->log, 1, "usb_get_paths_w0 '%s' config desc size strange\n",dpath);
				free(usbd);
				CloseHandle(handle);
				break;
			}
			if ((buf2 = calloc(1, totlen)) == NULL) {
				a1loge(p->log, ICOM_SYS, "usb_get_paths_w0 calloc of descriptor failed!\n");
				free(usbd);
				CloseHandle(handle);
				return ICOM_SYS;
			}
			
			/* Read the whole configuration descriptor */
			req.descriptor.type        = IUSB_DESC_TYPE_CONFIG;
			req.descriptor.recipient   = IUSB_REQ_RECIP_DEVICE;
			req.descriptor.index       = configix;
			req.descriptor.language_id = 0;
			req.timeout                = LIBUSBW1_DEFAULT_TIMEOUT;
			
			if (do_sync_io(handle, LIBUSB_IOCTL_GET_DESCRIPTOR, 
				                &req, sizeof(libusb_request), 
				                buf2, totlen, &retsz) != ICOM_OK
				                                     || retsz != totlen) {
				a1logd(p->log, 1, "usb_get_paths_w0 failed to read all configix %d descriptor\n",configix);
				free(buf2);
				free(usbd);
				CloseHandle(handle);
				break;
			}

			bp = buf2 + buf2[0];	/* Skip coniguration tag */
			zp = buf2 + totlen;		/* Past last bytes */

			/* We are at the first configuration. */
			/* Just read tags and keep track of where we are */
			ninfaces = 0;
			nep = 0;
			usbd->nifce = buf2[4];				/* number of interfaces */
			usbd->config = configno = buf2[5];	/* this configuration */
			for (;bp < zp; bp += bp[0]) {
				int ifaceno;
				if ((bp + 1) >= zp)
					break;			/* Hmm - bodgy, give up */
				if (bp[1] == IUSB_DESC_TYPE_INTERFACE) {
					ninfaces++;
					if ((bp + 2) >= zp)
						break;			/* Hmm - bodgy, give up */
					ifaceno = bp[2];	/* Get bInterfaceNumber */
				} else if (bp[1] == IUSB_DESC_TYPE_ENDPOINT) {
					nep++;
					if ((bp + 5) >= zp)
						break;			/* Hmm - bodgy */
					/* At first config - */
					/* record current nep and end point details */
					if (configno == 1) {	/* (if isn't actually needed...) */
						int ad = bp[2];
						nep10 = nep;
						usbd->EPINFO(ad).valid = 1;
						usbd->EPINFO(ad).addr = ad;
						usbd->EPINFO(ad).packetsize = usb2ushort(bp + 4);
						usbd->EPINFO(ad).type = bp[3] & IUSB_ENDPOINT_TYPE_MASK;
						usbd->EPINFO(ad).ifaceno = ifaceno;
						a1logd(p->log, 6, "set ep ad 0x%x packetsize %d type %d\n",ad,usbd->EPINFO(ad).packetsize,usbd->EPINFO(ad).type);
					}
				}
				/* Ignore other tags */
			}
			free(buf2);

			/* We only get here on the first config, so we're done. */
			break;

		}	/* Next configuration */
		if (nep10 == 0xffff) {			/* Hmm. Failed to find number of end points */
			a1logd(p->log, 1, "usb_get_paths_w0 failed to find number of end points\n");
			free(usbd);
			CloseHandle(handle);
			continue;
		}

		/* Check that we have an up to date kernel driver */
		req.timeout = LIBUSBW1_DEFAULT_TIMEOUT;
		if (do_sync_io(handle, LIBUSB_IOCTL_GET_VERSION, 
			                &req, sizeof(libusb_request), 
			                &req, sizeof(libusb_request), &retsz) != ICOM_OK
			                                     || retsz != sizeof(libusb_request)) {
			a1logd(p->log, 1, "usb_get_paths_w0 failed to read driver version info\n");
			free(usbd);
			CloseHandle(handle);
			continue;
		}
		if (req.version.major < 1
		 || req.version.major == 1 && (req.version.minor < 2
         || req.version.minor == 2 && req.version.micro < 6)) {
			a1loge(p->log, ICOM_VER, "usb_get_paths_w0 Must update %s System Driver to latest version!\n",inst_name(itype));
			free(usbd);
			CloseHandle(handle);
			return ICOM_VER;
		}
		CloseHandle(handle);

		/* Found a known instrument, and not found by a previous driver ? */
		if ((itype = inst_usb_match(vid, pid, nep10)) != instUnknown
		 && !p->check_usb_upto(p, upto, itype)) {
			char pname[400];

			a1logd(p->log, 1, "usb_get_paths_w0 found instrument vid 0x%04x, pid 0x%04x\n",vid,pid);

			/* Create a path/identification */
			sprintf(pname,"%s (%s)", dpath + 4, inst_name(itype));

			if ((usbd->dpath = strdup(dpath)) == NULL) {
				a1loge(p->log, ICOM_SYS, "usb_get_paths_w0 strdup path failed!\n");
				free(usbd);
				return ICOM_SYS;
			}

			/* Add the path and ep info to the list */
			if ((rv = p->add_usb(p, pname, vid, pid, nep10, usbd, itype)) != ICOM_OK)
				return rv;


		} else {
			free(usbd);
		}
	}

	return ICOM_OK;
}


/* Copy usb_idevice contents from icompaths to icom */
/* return icom error */
int usb_copy_usb_idevice_w0(icoms *d, icompath *s) {
	int i;
	if (s->usbd == NULL) { 
		d->usbd = NULL;
		return ICOM_OK;
	}
	if ((d->usbd = calloc(sizeof(struct usb_idevice), 1)) == NULL) {
		a1loge(d->log, ICOM_SYS, "usb_copy_usb_idevice_w0: malloc\n");
		return ICOM_SYS;
	}
	if ((d->usbd->dpath = strdup(s->usbd->dpath)) == NULL) {
		a1loge(d->log, ICOM_SYS, "usb_copy_usb_idevice_w0: malloc\n");
		return ICOM_SYS;
	}
	d->usbd->iserialno = s->usbd->iserialno;
	if (s->usbd->SerialNumber != NULL
	 && (d->usbd->SerialNumber = strdup(s->usbd->SerialNumber)) == NULL) {
		a1loge(d->log, ICOM_SYS, "usb_copy_usb_idevice: malloc\n");
		return ICOM_SYS;
	}

	/* Copy the current state & ep info */
	d->nconfig = s->usbd->nconfig;
	d->config = s->usbd->config;
	d->nifce = s->usbd->nifce;
	for (i = 0; i < 32; i++)
		d->ep[i] = s->usbd->ep[i];		/* Struct copy */
	return ICOM_OK;
}

/* Cleanup and then free a usb dev entry */
void usb_del_usb_idevice_w0(struct usb_idevice *usbd) {

	if (usbd == NULL)
		return;

	if (usbd->dpath != NULL)
		free(usbd->dpath);
	if (usbd->SerialNumber != NULL)
		free(usbd->SerialNumber);
	free(usbd);
}

/* Cleanup any USB specific icoms state */
void usb_del_usb_w0(icoms *p) {

	usb_del_usb_idevice(p->usbd);
}

/* Close an open USB port */
/* If we don't do this, the port and/or the device may be left in an unusable state. */
void usb_close_port_w0(icoms *p) {

	a1logd(p->log, 6, "usb_close_port_w0: called\n");

	if (p->is_open && p->usbd != NULL) {
		int iface, rv;

		/* Release all the interfaces */
		for (iface = 0; iface < p->nifce; iface++) {
			libusb_request req;

			memset(&req, 0, sizeof(req));
			req.intf.interface_number = iface;
			req.timeout = LIBUSBW1_DEFAULT_TIMEOUT;
		
			do_sync_io(p->usbd->handle, LIBUSB_IOCTL_RELEASE_INTERFACE, 
			                     &req, sizeof(libusb_request), NULL, 0, NULL);
		}

		/* Workaround for some bugs - reset device on close */
		if (p->uflags & icomuf_reset_before_close) {
			libusb_request req;

			a1logd(p->log, 6, "usb_close_port_w0: icomuf_reset_before_close\n");

			memset(&req, 0, sizeof(req));
			req.timeout = LIBUSBW1_DEFAULT_TIMEOUT;
		
			if ((rv = do_sync_io(p->usbd->handle, LIBUSB_IOCTL_RESET_DEVICE, 
			                     &req, sizeof(libusb_request), NULL, 0, NULL)) != ICOM_OK) {
				a1logd(p->log, 1, "usb_close_port_w0: reset returned %d\n",rv);
			}
			msec_sleep(500);	/* Things foul up unless we wait for the reset... */
		}
		CloseHandle(p->usbd->handle);

		a1logd(p->log, 6, "usb_close_port_w0: usb port has been released and closed\n");
	}
	p->is_open = 0;

	/* Find it and delete it from our static cleanup list */
	usb_delete_from_cleanup_list(p);
}

static void *urb_reaper(void *context);		/* Declare */

/* Open a USB port for all our uses. */
/* This always re-opens the port */
/* return icom error */
static int usb_open_port_w0(
icoms *p,
int    config,		/* Configuration number */
int    wr_ep,		/* Write end point */
int    rd_ep,		/* Read end point */
icomuflags usbflags,/* Any special handling flags */
int retries,		/* > 0 if we should retry set_configuration (100msec) */ 
char **pnames		/* List of process names to try and kill before opening */
) {
	int rv, tries = 0;
	a1logd(p->log, 8, "usb_open_port_w0: Make sure USB port is open, tries %d\n",retries);

	if (p->is_open)
		p->close_port(p);

	/* Make sure the port is open */
	if (!p->is_open) {
		int rv, i, iface;
		kkill_nproc_ctx *kpc = NULL;
		OSVERSIONINFO osver;

		if (config != 1) {
			/* NT defaults to config 1, and nothing currently needs any other config, */
			/* so we haven't implemented selecting a config yet... */
			a1loge(p->log, ICOM_NOTS, "usb_open_port_w0: native driver cant handle config %d\n",config);
			return ICOM_NOTS;

			msec_sleep(50);		/* Allow device some time to configure */
		}

		/* Do open retries */
		for (tries = 0; retries >= 0; retries--, tries++) {

			a1logd(p->log, 8, "usb_open_port_w0: About to open USB port '%s'\n",p->usbd->dpath);

			if (tries > 0) {
				//msec_sleep(i_rand(50,100));
				msec_sleep(77);
			}

			if ((p->usbd->handle = CreateFile(p->usbd->dpath, 0, 0, NULL, OPEN_EXISTING,
				             FILE_FLAG_OVERLAPPED, NULL)) == INVALID_HANDLE_VALUE) {
				a1logd(p->log, 8, "usb_open_port_w0: open '%s' config %d failed (%d) (Device being used ?)\n",p->usbd->dpath,config,GetLastError());
				if (retries <= 0) {
					if (kpc != NULL) {
						if (kpc->th->result < 0)
							a1logw(p->log, "usb_open_port_w0: killing competing processes failed\n");
						kpc->del(kpc); 
					}
					a1logw(p->log, "usb_open_port_w0: open '%s' config %d failed (%d) (Device being used ?)\n",p->usbd->dpath,config,GetLastError());
					return ICOM_SYS;
				}
				continue;
			} else if (p->debug)
				a1logd(p->log, 2, "usb_open_port_w0: open port '%s' succeeded\n",p->usbd->dpath);

			p->uflags = usbflags;

			/* We're done */
			break;
		}

		if (kpc != NULL)
			kpc->del(kpc); 

		/* We should only do a set configuration if the device has more than one */
		/* possible configuration and it is currently not the desired configuration, */
		/* but we should avoid doing a set configuration if the OS has already */
		/* selected the configuration we want, since two set configs seem to */
		/* mess up the Spyder2, BUT we can't do a get config because this */
		/* messes up the i1pro-D. */

		osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		osver.dwMajorVersion = 5;
		GetVersionEx(&osver);
		if (osver.dwMajorVersion >= 6 && osver.dwMinorVersion >= 2) {
			p->cconfig = 0;		/* Need to do set_congfig(1) on Win8 */
		} else {
			p->cconfig = 1;		/* Set by default to config 1 */
		}

		if (p->cconfig != config) {
			libusb_request req;

			memset(&req, 0, sizeof(libusb_request));
			req.configuration.configuration = config;
			req.timeout = LIBUSBW1_DEFAULT_TIMEOUT;
		
			if ((rv = do_sync_io(p->usbd->handle, LIBUSB_IOCTL_SET_CONFIGURATION, 
			                     &req, sizeof(libusb_request), NULL, 0, NULL)) != ICOM_OK) {
		
				a1loge(p->log, rv, "usb_open_port_w0: Setting port '%s' to config %d failed with %d\n",p->usbd->dpath,config,rv);
				return ICOM_SYS;
			}
			p->cconfig = config;
			a1logd(p->log, 6, "usb_open_port_w0: set config %d OK\n",config);
		}

		/* Claim all the interfaces */
		for (iface = 0; iface < p->nifce; iface++) {
			libusb_request req;

			memset(&req, 0, sizeof(libusb_request));
			req.intf.interface_number = iface;
			req.timeout = LIBUSBW1_DEFAULT_TIMEOUT;
		
			if ((rv = do_sync_io(p->usbd->handle, LIBUSB_IOCTL_CLAIM_INTERFACE, 
			                     &req, sizeof(libusb_request), NULL, 0, NULL)) != ICOM_OK) {
		
				a1loge(p->log, rv, "usb_open_port_w0: Claiming USB port '%s' interface %d failed with %d\n",p->usbd->dpath,iface,rv);
				return ICOM_SYS;
			}
		}

		/* Clear any errors */
		/* (Some I/F seem to hang if we do this, some seem to hang if we don't !) */
		if (!(p->uflags & icomuf_no_open_clear)) {
			for (i = 0; i < 32; i++) {
				if (!p->ep[i].valid)
					continue;
				p->usb_clearhalt(p, p->ep[i].addr); 
			}
		}

		/* Set "serial" coms values */
		p->wr_ep = wr_ep;
		p->rd_ep = rd_ep;
		p->rd_qa = p->EPINFO(rd_ep).packetsize;
		if (p->rd_qa == 0)
			p->rd_qa = 8;
		a1logd(p->log, 8, "usb_open_port_w0: 'serial' read quanta = packet size = %d\n",p->rd_qa);

		p->is_open = 1;
		a1logd(p->log, 8, "usb_open_port_w0: USB port is now open\n");

		/* If possible, read the device serial no */
		if (p->usbd->iserialno != 0) {
			unsigned char buf[IUSB_DESC_TYPE_STRING_MAX_SIZE]; 
			int se = 0;
			int retsz = 0;

			if (((se = icoms_usb_control_msg_w0(p, &retsz, 
				IUSB_REQ_DEV_TO_HOST | IUSB_REQ_TYPE_STANDARD | IUSB_REQ_RECIP_DEVICE,
				IUSB_REQ_GET_DESCRIPTOR,
	            (IUSB_DESC_TYPE_STRING << 8) | p->usbd->iserialno, 0,
				buf, IUSB_DESC_TYPE_STRING_MAX_SIZE, 200)) != ICOM_OK && se != ICOM_SHORT)
                                     || retsz < IUSB_DESC_TYPE_STRING_MIN_SIZE) {
				a1logd(p->log, 1, "usb_open_port failed to read device serial no with ICOM err 0x%x and retsz %d\n",se,retsz);
			} else {
				unsigned int slen = buf[0];

				if (slen > 2 && (slen & 1) == 0) {
					icmUTF16 buf2[IUSB_DESC_TYPE_STRING_MAX_SIZE/2];
					unsigned int j;
					size_t s8len;

					/* Convert to 16 bit native endian and add nul terminator */
					slen = (slen-2)/2;
					for (j = 0; j < slen; j++) {
						buf2[j] = usb2ushort(buf + 2 + j * 2);
					}
					buf2[j] = 0;

					s8len =icmUTF16toUTF8(NULL, NULL, buf2);	/* Compute UTF8 size */

					if ((p->usbd->SerialNumber = (char *) calloc(s8len, 1)) == NULL) {
						a1loge(p->log, ICOM_SYS, "usb_get_paths_w0 calloc failed!\n");
						usb_close_port(p);
						return ICOM_SYS;
					}
					icmUTF16toUTF8(NULL, (unsigned char *)p->usbd->SerialNumber, buf2);	/* Convert to UTF8 */
					a1logd(p->log, 6, "usb_open_port got USB serial no '%s'\n",p->usbd->SerialNumber);
				}
			}
		}
	}

	/* Install the cleanup signal handlers, and add to our cleanup list */
	usb_install_signal_handlers(p);

	return ICOM_OK;
}

/*  -------------------------------------------------------------- */

/* Our universal (non-control pipe) USB transfer function, used for rd/wr. */
/* It appears that we may return a timeout with valid characters. */
static int icoms_usb_transaction_w0(
	icoms *p,
	usb_cancelt *cancelt,		/* if !NULL, allows signalling another thread that op has started */
	int *transferred,
	icom_usb_trantype ttype,	/* transfer type */
	unsigned char endpoint,		/* 0x80 for control write, 0x00 for control read */
	unsigned char *buffer,
	int length,					/* Length to write/expected length to read */
	unsigned int timeout		/* In msec */
) {
	int rv = ICOM_OK;
	int dirw = (endpoint & IUSB_ENDPOINT_DIR_MASK) == IUSB_ENDPOINT_OUT ? 1 : 0;
    libusb_request req;
	OVERLAPPED olaps;
	DWORD xlength = 0;			/* Transferred length */

	in_usb_rw++;

	a1logd(p->log, 8, "icoms_usb_transaction_w0: req type 0x%x ep 0x%x size %d\n",ttype,endpoint,length);

	if (transferred != NULL)
		*transferred = 0;

	if (ttype != icom_usb_trantype_interrutpt
	 && ttype != icom_usb_trantype_bulk) {
		/* We only handle interrupt & bulk, not control */
		return ICOM_SYS;
	}

	memset(&olaps, 0, sizeof(olaps));	

	if ((olaps.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL)
		return ICOM_SYS;
	
	memset(&req, 0, sizeof(libusb_request));
	req.endpoint.endpoint = endpoint;

	if (!DeviceIoControl(p->usbd->handle, 
	                      dirw ? LIBUSB_IOCTL_INTERRUPT_OR_BULK_WRITE
	                      : LIBUSB_IOCTL_INTERRUPT_OR_BULK_READ,
	                      &req, sizeof(libusb_request), 
	                      buffer,
	                      length, &xlength, &olaps)) {
		if (GetLastError() != ERROR_IO_PENDING) {
			rv = dirw ? ICOM_USBW : ICOM_USBR;
			goto done;
		}

		if (cancelt != NULL) {
			amutex_lock(cancelt->cmtx);
			cancelt->hcancel = (void *)&endpoint;
			cancelt->state = 1;
			amutex_unlock(cancelt->condx);		/* Signal any thread waiting for IO start */
			amutex_unlock(cancelt->cmtx);
		}

		if (WaitForSingleObject(olaps.hEvent, timeout) == WAIT_TIMEOUT) {

			/* Cancel the operation, because it timed out */
			memset(&req, 0, sizeof(libusb_request));
			req.endpoint.endpoint = endpoint;
			req.timeout = LIBUSBW1_DEFAULT_TIMEOUT;
			do_sync_io(p->usbd->handle, LIBUSB_IOCTL_ABORT_ENDPOINT, 
				                   &req, sizeof(libusb_request), NULL, 0, NULL);
			rv = ICOM_TO;
		}

		if (!GetOverlappedResult(p->usbd->handle, &olaps, &xlength, TRUE)) {
			if (rv == ICOM_OK) {
				if (GetLastError() == ERROR_OPERATION_ABORTED)
					rv = ICOM_CANC;
				else
					rv = dirw ? ICOM_USBW : ICOM_USBR;
			}
		}
	}
done:;
	if (cancelt != NULL) {
		amutex_lock(cancelt->cmtx);
		cancelt->hcancel = (void *)NULL;
		if (cancelt->state == 0) {
			amutex_unlock(cancelt->condx);		/* Make sure this gets unlocked */
		}
		cancelt->state = 2;
		amutex_unlock(cancelt->cmtx);
	}

	CloseHandle(olaps.hEvent);

	if (transferred != NULL)
		*transferred = (int)xlength;

	/* The requested size wasn't transferred */
	if (rv == ICOM_OK  && xlength != length)
		rv |= ICOM_SHORT;

	if (in_usb_rw < 0)
		exit(0);

	in_usb_rw--;

	a1logd(p->log, 8, "coms_usb_transaction: returning err 0x%x and %d bytes\n",rv, xlength);

	return rv;
}


/* Our control message routine */
/* Return error icom error code */
static int icoms_usb_control_msg_w0(
	icoms *p,
	int *transferred,
	int requesttype,
	int request,
	int value,
	int index,
	unsigned char *bytes,
	int size, 
	int timeout
) {
	int rv = ICOM_OK;
	int dirw = (requesttype & IUSB_REQ_DIR_MASK) == IUSB_REQ_HOST_TO_DEV ? 1 : 0;
    libusb_request req;
	unsigned char *obuf = (unsigned char *)&req;
	int osize = sizeof(libusb_request);
	unsigned char *ibuf = bytes;
	int isize = size;
    int ioctl = 0;
	int retsz = 0;

	a1logd(p->log, 8, "icoms_usb_control_msg_w0: type 0x%x req 0x%x size %d\n",requesttype,request,size);

	if (transferred != NULL)
		*transferred = 0;

	memset(&req, 0, sizeof(libusb_request));
    req.timeout = timeout;

	/* We need to treat each request type as a different IOCTL */
	switch (requesttype & IUSB_REQ_TYPE_MASK) {

		case IUSB_REQ_TYPE_STANDARD:

			switch (request) {
				case IUSB_REQ_GET_STATUS:
					req.status.recipient = requesttype & IUSB_REQ_RECIP_MASK;
					req.status.index = index;
					ioctl = LIBUSB_IOCTL_GET_STATUS;
					break;

				case IUSB_REQ_CLEAR_FEATURE:
					req.feature.recipient = requesttype & IUSB_REQ_RECIP_MASK;
					req.feature.feature = value;
					req.feature.index = index;
					ioctl = LIBUSB_IOCTL_CLEAR_FEATURE;
					break;

				case IUSB_REQ_SET_FEATURE:
					req.feature.recipient = requesttype & IUSB_REQ_RECIP_MASK;
					req.feature.feature = value;
					req.feature.index = index;
					ioctl = LIBUSB_IOCTL_SET_FEATURE;
					break;

				case IUSB_REQ_GET_DESCRIPTOR:
					req.descriptor.recipient = requesttype & IUSB_REQ_RECIP_MASK;
					req.descriptor.type = (value >> 8) & 0xFF;
					req.descriptor.index = value & 0xFF;
					req.descriptor.language_id = index;
					ioctl = LIBUSB_IOCTL_GET_DESCRIPTOR;
					break;

				case IUSB_REQ_SET_DESCRIPTOR:
					req.descriptor.recipient = requesttype & IUSB_REQ_RECIP_MASK;
					req.descriptor.type = (value >> 8) & 0xFF;
					req.descriptor.index = value & 0xFF;
					req.descriptor.language_id = index;
					ioctl = LIBUSB_IOCTL_SET_DESCRIPTOR;
					break;

				case IUSB_REQ_GET_CONFIGURATION:
					ioctl = LIBUSB_IOCTL_GET_CONFIGURATION;
					break;

				case IUSB_REQ_SET_CONFIGURATION:
					req.configuration.configuration = value;
					ioctl = LIBUSB_IOCTL_SET_CONFIGURATION;
					break;

				case IUSB_REQ_GET_INTERFACE:
					req.intf.interface_number = index;
					ioctl = LIBUSB_IOCTL_GET_INTERFACE;
					break;

				case IUSB_REQ_SET_INTERFACE:
					req.intf.interface_number = index;
					req.intf.altsetting_number = value;
					ioctl = LIBUSB_IOCTL_SET_INTERFACE;
					break;

				default:
					return ICOM_SYS; 
			}
			break;

		case IUSB_REQ_TYPE_VENDOR:
		case IUSB_REQ_TYPE_CLASS:

			req.vendor.type = (requesttype & IUSB_REQ_TYPE_MASK) >> IUSB_REQ_TYPE_SHIFT;
			req.vendor.recipient = requesttype & IUSB_REQ_RECIP_MASK;
			req.vendor.request = request;
			req.vendor.value = value;
			req.vendor.index = index;

			if (dirw)
				ioctl = LIBUSB_IOCTL_VENDOR_WRITE;
			else
				ioctl = LIBUSB_IOCTL_VENDOR_READ;
			break;

		case IUSB_REQ_TYPE_RESERVED:
		default:
			return ICOM_SYS;
	}

	/* If we're writing the data, append it to the req */
    if (dirw) {
        osize = sizeof(libusb_request) + size;
		if ((obuf = calloc(1, osize)) == NULL) {
			a1loge(p->log, ICOM_SYS, "icoms_usb_control_msg_w0: calloc failed\n");
			return ICOM_SYS;
		}

        memcpy(obuf, &req, sizeof(libusb_request));
        memcpy(obuf + sizeof(libusb_request), bytes, size);
        ibuf = NULL;
        isize = 0;
    }

    if ((rv = do_sync_io(p->usbd->handle, ioctl, obuf, osize, ibuf, isize, &retsz))
		                                                                     != ICOM_OK) {
		if (dirw)
			free(obuf);
		return rv;
    }

	if (dirw) {
		free(obuf);
		retsz = size;
	}

	if (transferred != NULL)	/* Adjust for header size requested */
		*transferred = retsz;

	/* The requested size wasn't transferred */
	if (rv == ICOM_OK  && retsz != size)
		rv |= ICOM_SHORT;

	a1logd(p->log, 8, "icoms_usb_control_msg_w0: returning err 0x%x and %d bytes\n",rv, *transferred);
	return rv;
}

/* Cancel i/o operation in another thread. */
/* Only Vista has CancelIoEx that can cancel a single operation, */
/* so we cancel the io to the end point, which will */
/* achieve what we want. */
int icoms_usb_cancel_io_w0(
	icoms *p,
	usb_cancelt *cancelt
) {
	int rv = ICOM_OK;
	amutex_lock(cancelt->cmtx);
	if (cancelt->hcancel != NULL) {
		libusb_request req;

		memset(&req, 0, sizeof(libusb_request));
		req.endpoint.endpoint = *((unsigned char *)cancelt->hcancel);
		req.timeout = LIBUSBW1_DEFAULT_TIMEOUT;

		if ((rv = do_sync_io(p->usbd->handle, LIBUSB_IOCTL_ABORT_ENDPOINT, 
				                   &req, sizeof(libusb_request), NULL, 0, NULL)) != ICOM_OK) {
			a1logd(p->log, 1, "icoms_usb_cancel_io_w0: failed with 0x%x\n",rv);
		}
	}
	amutex_unlock(cancelt->cmtx);

	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Reset and end point data toggle to 0 */
int icoms_usb_resetep_w0(
	icoms *p,
	int ep					/* End point address */
) {
	libusb_request req;
	int rv = ICOM_OK;

	memset(&req, 0, sizeof(libusb_request));
	req.endpoint.endpoint = ep;
	req.timeout = LIBUSBW1_DEFAULT_TIMEOUT;

	if ((rv = do_sync_io(p->usbd->handle, LIBUSB_IOCTL_RESET_ENDPOINT, &req, 
	                    sizeof(libusb_request), NULL, 0, NULL)) != ICOM_OK) {
		a1logd(p->log, 1, "icoms_usb_resetep_w0 failed with %d\n",rv);
		return rv;
	}
	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Clear a halt on an end point */
/* (Actually does a resetep) */
int icoms_usb_clearhalt_w0(
	icoms *p,
	int ep					/* End point address */
) {
	libusb_request req;
	int rv = ICOM_OK;

	memset(&req, 0, sizeof(libusb_request));
	req.endpoint.endpoint = ep;
	req.timeout = LIBUSBW1_DEFAULT_TIMEOUT;

	if ((rv = do_sync_io(p->usbd->handle, LIBUSB_IOCTL_RESET_ENDPOINT, &req, 
	                    sizeof(libusb_request), NULL, 0, NULL)) != ICOM_OK) {
		a1logd(p->log, 1, "icoms_resetpipe/clearhalt_w0 failed with %d\n",rv);
		return rv;
	}
	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef NEVER
			libusb_request req;
			unsigned char buf[1] = { 0xff };

			memset(&req, 0, sizeof(libusb_request));
			req.timeout = LIBUSBW1_DEFAULT_TIMEOUT;
		
			if ((rv = do_sync_io(p->usbd->handle, LIBUSB_IOCTL_GET_CONFIGURATION, 
			                     &req, sizeof(libusb_request), buf, 1, NULL)) != ICOM_OK) {
		
				a1logd(p->log, 1, "usb_open_port_w0: Getting port '%s' configuration failed with %d\n",p->usbd->dpath,rv);
				/* Ignore error */
			} else {
				a1logd(p->log, 1, "usb_open_port_w0: current config = %d\n",(int)buf[0]);
			}
			config = buf[0];
#endif // NEVER

