
/* General USB I/O support for MSWin using the UsbDk system driver. */
/* This file is conditionaly #included into usbio.c */
/* Based on usbio_w0.c */

# pragma message("######### !!!!! usbio_usbdk.c compiled !!!!! ########")
 
/* 
 * Argyll Color Management System
 *
 * Author: Graeme W. Gill
 * Date:   2023/9/25
 *
 * Copyright 2006 - 2023 Graeme W. Gill
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

#include "conv.h"

#pragma message("!!!!!!!!!!!!!!!!!!!! usbio_dk defined !!!!!!!!!!!!!!!!!!!!!!!!!!!!")

#undef DEBUG		/* Turn on debug messages */

// See example d:/src/libusb/libusb_1026/libusb/os/windows_usbdk.c

/* --------------------------------------------------- */
/* UsbDk functions */

static UsbDk_inited = 0;

static UsbDk_InstallDriver_t					UsbDk_InstallDriver = NULL;
static UsbDk_UninstallDriver_t					UsbDk_UninstallDriver = NULL;
static UsbDk_GetDevicesList_t					UsbDk_GetDevicesList = NULL;
static UsbDk_ReleaseDevicesList_t				UsbDk_ReleaseDevicesList = NULL;
static UsbDk_GetConfigurationDescriptor_t		UsbDk_GetConfigurationDescriptor = NULL;
static UsbDk_ReleaseConfigurationDescriptor_t	UsbDk_ReleaseConfigurationDescriptor = NULL;
static UsbDk_StartRedirect_t					UsbDk_StartRedirect = NULL;
static UsbDk_StopRedirect_t						UsbDk_StopRedirect = NULL;
static UsbDk_WritePipe_t						UsbDk_WritePipe = NULL;
static UsbDk_ReadPipe_t							UsbDk_ReadPipe = NULL;
static UsbDk_AbortPipe_t						UsbDk_AbortPipe = NULL;
static UsbDk_ResetPipe_t						UsbDk_ResetPipe = NULL;
static UsbDk_SetAltsetting_t					UsbDk_SetAltsetting = NULL;
static UsbDk_ResetDevice_t						UsbDk_ResetDevice = NULL;
static UsbDk_GetRedirectorSystemHandle_t		UsbDk_GetRedirectorSystemHandle = NULL;

/* Make sure UsbDk is installed, and get the function pointers. */
/* Return nz on failure */
static int init_UsbDk(icompaths *p) {
	char *pf = NULL;
	char path[1024];
	HMODULE lib;

	if (UsbDk_inited)
		return 0;

	a1logd(p->log, 6, "init_UsbDk called:\n");

	// ~8 ???
	// Should check registry for a UsbDk install, and check the version.
	// If it is not a version with bug fixes, uninstall it
	// HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\UsbDk
	// HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\UsbDk\Parameters\Wdf
	
	/* Look for the UsbDk .dll */
	if ((pf = getenv("PROGRAMFILES")) == NULL)
		pf = "C:\\Program Files";

	strcpy(path, pf);
	strcat(path, "/UsbDk Runtime Library/UsbDkHelper.dll");

	if ((lib = LoadLibrary(path)) == NULL) {
		// ~8 Should try and install ../usb/UsbDk_1.0.22_x64.msi or ../usb/UsbDk_1.0.22_x86.msi
		// try and install it without the drivers running
		a1logd(p->log, 1, "init_UsbDk didn't find library '%s'\n",path);
		return 1;
	}

	a1logd(p->log, 7, "init_UsbDk found library\n");

	UsbDk_InstallDriver = (UsbDk_InstallDriver_t)GetProcAddress(lib, "UsbDk_InstallDriver");
	UsbDk_UninstallDriver = (UsbDk_UninstallDriver_t)GetProcAddress(lib, "UsbDk_UninstallDriver");
	UsbDk_GetDevicesList = (UsbDk_GetDevicesList_t)GetProcAddress(lib, "UsbDk_GetDevicesList");
	UsbDk_ReleaseDevicesList = (UsbDk_ReleaseDevicesList_t)
	                                          GetProcAddress(lib, "UsbDk_ReleaseDevicesList");
	UsbDk_GetConfigurationDescriptor = (UsbDk_GetConfigurationDescriptor_t)
	                                  GetProcAddress(lib, "UsbDk_GetConfigurationDescriptor");
	UsbDk_ReleaseConfigurationDescriptor = (UsbDk_ReleaseConfigurationDescriptor_t)
	                              GetProcAddress(lib, "UsbDk_ReleaseConfigurationDescriptor");
	UsbDk_StartRedirect = (UsbDk_StartRedirect_t)GetProcAddress(lib, "UsbDk_StartRedirect");
	UsbDk_StopRedirect = (UsbDk_StopRedirect_t)GetProcAddress(lib, "UsbDk_StopRedirect");
	UsbDk_WritePipe = (UsbDk_WritePipe_t)GetProcAddress(lib, "UsbDk_WritePipe");
	UsbDk_ReadPipe = (UsbDk_ReadPipe_t)GetProcAddress(lib, "UsbDk_ReadPipe");
	UsbDk_AbortPipe = (UsbDk_AbortPipe_t)GetProcAddress(lib, "UsbDk_AbortPipe");
	UsbDk_ResetPipe = (UsbDk_ResetPipe_t)GetProcAddress(lib, "UsbDk_ResetPipe");
	UsbDk_SetAltsetting = (UsbDk_SetAltsetting_t)GetProcAddress(lib, "UsbDk_SetAltsetting");
	UsbDk_ResetDevice = (UsbDk_ResetDevice_t)GetProcAddress(lib, "UsbDk_ResetDevice");
	UsbDk_GetRedirectorSystemHandle = (UsbDk_GetRedirectorSystemHandle_t)
	                                 GetProcAddress(lib, "UsbDk_GetRedirectorSystemHandle");

	/* Did all that work ? */
	if (UsbDk_InstallDriver == NULL
	 || UsbDk_UninstallDriver == NULL
	 || UsbDk_GetDevicesList == NULL
	 || UsbDk_ReleaseDevicesList == NULL
	 || UsbDk_GetConfigurationDescriptor == NULL
	 || UsbDk_ReleaseConfigurationDescriptor == NULL
	 || UsbDk_StartRedirect == NULL
	 || UsbDk_StopRedirect == NULL
	 || UsbDk_WritePipe == NULL
	 || UsbDk_ReadPipe == NULL
	 || UsbDk_AbortPipe == NULL
	 || UsbDk_ResetPipe == NULL
	 || UsbDk_SetAltsetting == NULL
	 || UsbDk_ResetDevice == NULL
	 || UsbDk_GetRedirectorSystemHandle == NULL) {
		a1logd(p->log, 1, "init_UsbDk failed to find all function\n");
		return 1;
	}

	// ~8 should check if the system driver is running ???
	// Can do this by checking service manager ?
	// offer to start drivers. If no, fail init
	// Start drivers using UsbDk_InstallDriver() ?
	// Ask for confirm, if no confirm in 10 seconds, uninstall driver and fail init

	a1logd(p->log, 6, "init_UsbDk done\n");

	return 0;
}

/* Return a diagnostic string from the ID */
/* The returned buffer is static. */
static char *ID2str(USB_DK_DEVICE_ID *ID) {
	static char buf[200];

	if ((icmUTF16toUTF8(NULL, NULL, ID->DeviceID)
	  + icmUTF16toUTF8(NULL, NULL, ID->InstanceID)
	  + 1) > 200) {
		sprintf(buf, "USBDK_ID too long for buffer!");
		return buf;
	}

	icmUTF16toUTF8(NULL, buf, ID->DeviceID);
	strcat(buf, "-"); 
	icmUTF16toUTF8(NULL, buf + strlen(buf), ID->InstanceID);

	return buf;
}

/* Add an OVERLAP cleanup record */
static add_olr(icoms *p, struct usbdk_olr *olr) {
	
	EnterCriticalSection(&p->usbd->lock);
	olr->next = p->usbd->olr;
	if (p->usbd->olr != NULL)
		p->usbd->olr->last = &olr->next;
	p->usbd->olr = olr;
	olr->last = &p->usbd->olr;
	LeaveCriticalSection(&p->usbd->lock);
}

/* Remove an OVERLAP cleanup record */
static rem_olr(icoms *p, struct usbdk_olr *olr) {
	
	EnterCriticalSection(&p->usbd->lock);
	*olr->last = olr->next;
	if (olr->next != NULL)
		olr->next->last = olr->last;
	olr->last = NULL;
	olr->next = NULL;
	LeaveCriticalSection(&p->usbd->lock);
}

/* --------------------------------------------------- */

/* Add paths to USB connected device */
/* Return an icom error */
int usb_get_paths_dk(
icompaths *p 
) {
	struct usb_idevice *usbd = NULL;
	USB_DK_DEVICE_INFO *DevicesArray;
	ULONG i, NumberDevices;
	int upto;

	a1logd(p->log, 5, "usb_get_paths_dk:\n");

	if (init_UsbDk(p)) {
		return ICOM_SYS;
	}

	if (!UsbDk_GetDevicesList(&DevicesArray, &NumberDevices)) {
		a1logd(p->log, 1, "UsbDk_GetDevicesList failed\n");
		return ICOM_SYS;
	}
	a1logd(p->log, 6, "UsbDk_GetDevicesList got %d devices\n",NumberDevices);

	//    USB_DK_DEVICE_ID ID;
	//    ULONG64 FilterID;
	//    ULONG64 Port;
	//    ULONG64 Speed;
	//    USB_DEVICE_DESCRIPTOR DeviceDescriptor;
	
	//USB_DEVICE_DESCRIPTOR
	//    UCHAR   bLength;
	//    UCHAR   bDescriptorType;
	//    USHORT  bcdUSB;
	//    UCHAR   bDeviceClass;
	//    UCHAR   bDeviceSubClass;
	//    UCHAR   bDeviceProtocol;
	//    UCHAR   bMaxPacketSize0;
	//    USHORT  idVendor;
	//    USHORT  idProduct;
	//    USHORT  bcdDevice;
	//    UCHAR   iManufacturer;
	//    UCHAR   iProduct;
	//    UCHAR   iSerialNumber;
	//    UCHAR   bNumConfigurations;

	upto = p->get_cur_count(p);

	for (i = 0; i < NumberDevices; i++) {
		unsigned int vid, pid, nep10 = 0xffff;
		unsigned int nconfig, configix;
		devType itype;

		vid = DevicesArray[i].DeviceDescriptor.idVendor; 
		pid = DevicesArray[i].DeviceDescriptor.idProduct; 
		nconfig = DevicesArray[i].DeviceDescriptor.bNumConfigurations; 

		/*
			iManufacturer = DevicesArray[i].DeviceDescriptor.iManufacturer;
			iProduct      = DevicesArray[i].DeviceDescriptor.iProduct;
			iSerialNumber = DevicesArray[i].DeviceDescriptor.iSerialNumber;
		*/
# pragma message("######### usbio_dk.c Missing SerialNumber read code !!!!! ########")

		a1logd(p->log, 6, "usb_get_paths_dk: checking vid 0x%04x, pid 0x%04x\n",vid,pid);

		/* Do a preliminary match */
		if ((itype = inst_usb_match(vid, pid, 0)) == instUnknown) {
			a1logd(p->log, 6 , "usb_get_paths_dk: instrument not reconized\n");
			continue;
		}

		/* Allocate an idevice so that we can fill in the end point information */
		if ((usbd = (struct usb_idevice *) calloc(sizeof(struct usb_idevice), 1)) == NULL) {
			a1loge(p->log, ICOM_SYS, "usb_get_paths_dk: calloc failed!\n");
			return ICOM_SYS;
		}

		usbd->nconfig = nconfig;
		usbd->is_dk = 1;
		usbd->ID = DevicesArray[i].ID;

		/* Read the configuration descriptors looking for the first configuration, first interface, */
		/* and extract the number of end points for each configuration */
		for (configix = 0; configix < nconfig; configix++) {
			USB_DK_CONFIG_DESCRIPTOR_REQUEST Request;
			USB_CONFIGURATION_DESCRIPTOR *Descriptor;
			ULONG Length;
			int configno, totlen;
			unsigned char *bp, *zp;
			unsigned int nifce, ifcount, nep;

			/* Read the configuration descriptor */
			Request.ID = DevicesArray[i].ID;
			Request.Index = configix;
			
    		if (!UsbDk_GetConfigurationDescriptor(&Request, &Descriptor, &Length)) {
				a1logd(p->log, 1, "usb_get_paths_dk: '%s' failed to read configix %d descriptor\n",
				                                                    ID2str(&usbd->ID), configix);
				free(usbd);
				break;
			}
			// USB_CONFIGURATION_DESCRIPTOR
			//     UCHAR   bLength;
			//     UCHAR   bDescriptorType;
			//     USHORT  wTotalLength;
			//     UCHAR   bNumInterfaces;
			//     UCHAR   bConfigurationValue;
			//     UCHAR   iConfiguration;			Index of string descriptor
			//     UCHAR   bmAttributes;
			//     UCHAR   MaxPower;
			//     UCHAR   ....						Interface & Endpoint descriptors
			nifce = Descriptor->bNumInterfaces;			/* number of interfaces */
			configno = Descriptor->bConfigurationValue;	/* Configuration number */

			if (configno != 1) {
				UsbDk_ReleaseConfigurationDescriptor(Descriptor);
				continue;
			}

			if ((totlen = Descriptor->wTotalLength) < 6) {
				a1logd(p->log, 1, "usb_get_paths_dk: '%s' config desc size strange\n",ID2str(&usbd->ID));
				UsbDk_ReleaseConfigurationDescriptor(Descriptor);
				free(usbd);
				break;
			}

			bp = ((unsigned char *)Descriptor) + Descriptor->bLength;	/* Skip coniguration tag */
			zp = ((unsigned char *)Descriptor) + totlen;		/* Past last bytes */

			/* We are at the first configuration. */
			/* Just read tags and keep track of where we are */
			ifcount = 0;
			nep = 0;
			usbd->nifce = nifce;				/* number of interfaces */
			usbd->config = configno;			/* this configuration */
			for (;bp < zp; bp += bp[0]) {
				int ifaceno;
				if ((bp + 1) >= zp)
					break;			/* Hmm - bodgy, give up */
				if (bp[1] == IUSB_DESC_TYPE_INTERFACE) {
					ifcount++;
					if ((bp + 2) >= zp)
						break;			/* Hmm - bodgy, give up */
					ifaceno = bp[2];	/* Get bInterfaceNumber */
				} else if (bp[1] == IUSB_DESC_TYPE_ENDPOINT) {
					nep++;
					if ((bp + 5) >= zp)
						break;			/* Hmm - bodgy */
					/* At first config - */
					/* record current nep and end point details */
					if (configno == 1) {		/* (if isn't actually needed...) */
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
			UsbDk_ReleaseConfigurationDescriptor(Descriptor);

			/* We only get here on the first config, so we're done. */
			break;

		}	/* Next configuration */
		if (nep10 == 0xffff) {			/* Hmm. Failed to find number of end points */
			a1logd(p->log, 1, "usb_get_paths_dk: failed to find number of end points\n");
			free(usbd);
			continue;
		}

		/* Found a known instrument, and not found by a previous driver ? */
		if ((itype = inst_usb_match(vid, pid, nep10)) != instUnknown
		 && !p->check_usb_upto(p, upto, itype)) {
			char pname[400];
			int rv;

			a1logd(p->log, 1, "usb_get_paths_dk: found instrument vid 0x%04x, pid 0x%04x\n",vid,pid);

			/* Create a path/identification */
			sprintf(pname,"%s (%s)", ID2str(&usbd->ID), inst_name(itype));

			/* Add the path and ep info to the list */
			if ((rv = p->add_usb(p, pname, vid, pid, nep10, usbd, itype)) != ICOM_OK) {
				free(usbd);
				UsbDk_ReleaseDevicesList(DevicesArray);
				return rv;
			}


		} else {
			free(usbd);
		}
	}	/* Next device */

	UsbDk_ReleaseDevicesList(DevicesArray);

	return ICOM_OK;
}


/* Copy usb_idevice contents from icompaths to icom */
/* return icom error */
int usb_copy_usb_idevice_dk(icoms *d, icompath *s) {
	int i;
	if (s->usbd == NULL) { 
		d->usbd = NULL;
		return ICOM_OK;
	}
	if ((d->usbd = calloc(sizeof(struct usb_idevice), 1)) == NULL) {
		a1loge(d->log, ICOM_SYS, "usb_copy_usb_idevice_dk: malloc\n");
		return ICOM_SYS;
	}

	d->usbd->is_dk = s->usbd->is_dk;
	d->usbd->ID = s->usbd->ID;

	if ((d->usbd->SerialNumber = strdup(s->usbd->SerialNumber)) == NULL) {
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
void usb_del_usb_idevice_dk(struct usb_idevice *usbd) {

	if (usbd == NULL)
		return;
	if (usbd->SerialNumber != NULL)
		free(usbd->SerialNumber);
	free(usbd);
}

/* Cleanup any USB specific icoms state */
void usb_del_usb_dk(icoms *p) {

	usb_del_usb_idevice(p->usbd);
}

/* Close an open USB port */
/* If we don't do this, the port and/or the device may be left in an unusable state. */
void usb_close_port_dk(icoms *p) {
	a1logd(p->log, 6, "usb_close_port_dk: called\n");

	if (p->is_open && p->usbd != NULL) {
		HANDLE handle = p->usbd->handle;
		int i;
		struct usbdk_olr *olr;

		p->usbd->handle = INVALID_HANDLE_VALUE;

		/* We don't need to release interfaces */

		/* Be nice and clean up. This helps workaround issue #105... */
		/* Make sure any outstanding transfers are cancelled. */
		a1logd(p->log, 6, "usb_close_port_dk: cancelling transfers\n");
		for (i = 0; i < 32; i++) {
			if (!p->ep[i].valid)
				continue;
			UsbDk_AbortPipe(handle, (ULONG64)p->ep[i].addr);
		}

		/* If there are pending operations, complete the OVERLAPPED, */
		/* or else the driver may lock up and the system might need a re-boot... */
		for (olr = p->usbd->olr; olr != NULL; olr = olr->next) {
			DWORD xlength;
			if (olr->olaps.hEvent != NULL) {
				a1logd(p->log, 6, "usb_close_port_dk: doing GetOverlappedResult\n");
				GetOverlappedResult(p->usbd->syshandle, &olr->olaps, &xlength, TRUE);
				CloseHandle(olr->olaps.hEvent);
			}
		}

#ifdef NEVER 	/* We don't need to do this, since UsbDk CYCLEs the port anyway... */
		/* Workaround for some bugs - reset device on close */
		if (p->uflags & icomuf_reset_before_close) {
			a1logd(p->log, 6, "usb_close_port_dk: icomuf_reset_before_close\n");

			if (!UsbDk_ResetDevice(handle)) {
				a1logd(p->log, 1, "usb_close_port_dk: reset failed\n");
			}
			msec_sleep(500);	/* Things foul up unless we wait for the reset... */
		}
#endif

		if (!UsbDk_StopRedirect(handle)) {
			a1logd(p->log, 1, "usb_close_port_dk failed\n");
		}

		a1logd(p->log, 6, "usb_close_port_dk: usb port has been released and closed\n");
	}
	p->is_open = 0;

	/* Find it and delete it from our static cleanup list */
	usb_delete_from_cleanup_list(p);
}

static void *urb_reaper(void *context);		/* Declare */

/* Open a USB port for all our uses. */
/* This always re-opens the port */
/* return icom error */
static int usb_open_port_dk(
icoms *p,
int    config,		/* Configuration number */
int    wr_ep,		/* Write end point */
int    rd_ep,		/* Read end point */
icomuflags usbflags,/* Any special handling flags */
int retries,		/* > 0 if we should retry set_configuration (100msec) */ 
char **pnames		/* List of process names to try and kill before opening */
) {
	int rv, tries = 0;
	a1logd(p->log, 8, "usb_open_port_dk: Make sure USB port is open, tries %d\n",retries);

	if (p->is_open)
		p->close_port(p);

	/* Make sure the port is open */
	if (!p->is_open) {
		int rv, i, iface;
		kkill_nproc_ctx *kpc = NULL;

		if (config != 1) {
			/* NT defaults to config 1, and nothing currently needs any other config, */
			/* so we haven't implemented selecting a config yet... */
			a1loge(p->log, ICOM_NOTS, "usb_open_port_dk: native driver cant handle config %d\n",config);
			return ICOM_NOTS;

			msec_sleep(50);		/* Allow device some time to configure */
		}

		InitializeCriticalSection(&p->usbd->lock);

		/* Do open retries */
		for (tries = 0; retries >= 0; retries--, tries++) {

			a1logd(p->log, 8, "usb_open_port_dk: About to open USB port '%s'\n",ID2str(&p->usbd->ID));

			if (tries > 0) {
				//msec_sleep(i_rand(50,100));
				msec_sleep(77);
			}

			if ((p->usbd->handle = UsbDk_StartRedirect(&p->usbd->ID)) == INVALID_HANDLE_VALUE) {
				a1logd(p->log, 8, "usb_open_port_dk: open '%s' config %d failed (%d) (Device being used ?)\n",ID2str(&p->usbd->ID),config,GetLastError());
				if (retries <= 0) {
					if (kpc != NULL) {
						if (kpc->th->result < 0)
							a1logw(p->log, "usb_open_port_dk: killing competing processes failed\n");
						kpc->del(kpc); 
					}
					a1logw(p->log, "usb_open_port_dk: open '%s' config %d failed (%d) (Device being used ?)\n",ID2str(&p->usbd->ID),config,GetLastError());
					return ICOM_SYS;
				}
				continue;
			} else if (p->debug)
				a1logd(p->log, 2, "usb_open_port_dk: open port '%s' succeeded\n",ID2str(&p->usbd->ID));

			p->usbd->syshandle = UsbDk_GetRedirectorSystemHandle(p->usbd->handle);
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

		/* For the moment, assume that the configuration is correct... */

		/* UsbDk doesn't seem to support claiming of interfaces. */
		/* Perhaps this is because it is not set up to share a USB device */
		/* - it grabs exclusive use. */

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
		a1logd(p->log, 8, "usb_open_port_dk: 'serial' read quanta = packet size = %d\n",p->rd_qa);

		p->is_open = 1;
		a1logd(p->log, 8, "usb_open_port_dk: USB port is now open\n");
	}

	/* Install the cleanup signal handlers, and add to our cleanup list */
	usb_install_signal_handlers(p);

	return ICOM_OK;
}

/*  -------------------------------------------------------------- */

/* Our universal (non-control pipe) USB transfer function, used for rd/wr. */
/* It appears that we may return a timeout with valid characters. */
static int icoms_usb_transaction_dk(
	icoms *p,
	usb_cancelt *cancelt,		/* if !NULL, allows signalling another thread that op has started */
	int *transferred,
	icom_usb_trantype ttype,	/* transfer type */
	unsigned char endpoint,		/* 0x80 for write, 0x00 for read */
	unsigned char *buffer,
	int length,					/* Length to write/expected length to read */
	unsigned int timeout		/* In msec */
) {
	USB_DK_TRANSFER_REQUEST Request = { 0 };
	TransferResult result;
	int rv = ICOM_OK;
	struct usbdk_olr olr;
	int dirw = (endpoint & IUSB_ENDPOINT_DIR_MASK) == IUSB_ENDPOINT_OUT ? 1 : 0;
	DWORD xlength;
	int retsz = 0;

	in_usb_rw++;

	a1logd(p->log, 8, "icoms_usb_transaction_dk: req type 0x%x ep 0x%x size %d\n",ttype,endpoint,length);

	if (transferred != NULL)
		*transferred = 0;

	if (ttype != icom_usb_trantype_interrutpt
	 && ttype != icom_usb_trantype_bulk) {
		/* We only handle interrupt & bulk, not control */
		return ICOM_SYS;
	}

	Request.EndpointAddress = endpoint;
	Request.Buffer = buffer;
	Request.BufferLength = length;
	Request.TransferType = icom_usb_trantype_interrutpt ? InterruptTransferType : BulkTransferType;

	memset(&olr, 0, sizeof(struct usbdk_olr));

	if ((olr.olaps.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL)
		return ICOM_SYS;
	
	add_olr(p, &olr);

	if (dirw)
		result = UsbDk_WritePipe(p->usbd->handle, &Request, &olr.olaps);
	else
		result = UsbDk_ReadPipe(p->usbd->handle, &Request, &olr.olaps);

	if (result == TransferFailure) {
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

	if (WaitForSingleObject(olr.olaps.hEvent, timeout) == WAIT_TIMEOUT) {
		/* Cancel the operation, because it timed out */
		UsbDk_AbortPipe(p->usbd->handle, (ULONG64)endpoint);
		rv = ICOM_TO;
	}

	if (!GetOverlappedResult(p->usbd->handle, &olr.olaps, &xlength, TRUE)) {
		if (rv == ICOM_OK) {
			if (GetLastError() == ERROR_OPERATION_ABORTED)
				rv = ICOM_CANC;
			else
				rv = dirw ? ICOM_USBW : ICOM_USBR;
		}
	}
done:;
	if (cancelt != NULL) {
		amutex_lock(cancelt->cmtx);
		cancelt->hcancel = (void *)NULL;
		if (cancelt->state == 0)
			amutex_unlock(cancelt->condx);		/* Make sure this gets unlocked */
		cancelt->state = 2;
		amutex_unlock(cancelt->cmtx);
	}

	CloseHandle(olr.olaps.hEvent);
	olr.olaps.hEvent = NULL;
	rem_olr(p, &olr);

	/* xlength doesn't seem to correspond with what UsbDk did, BytesTransferred does. */
	retsz = (int)Request.Result.GenResult.BytesTransferred;

	if (transferred != NULL)
		*transferred = retsz;

	/* The requested size wasn't transferred */
	if (rv == ICOM_OK  && retsz != length)
		rv |= ICOM_SHORT;

	if (in_usb_rw < 0) {		/* We're being cleaned up */
		a1logd(p->log, 8, "coms_usb_transaction: aborting because in_usb_rw = %dn",in_usb_rw);
		exit(0);
	}

	in_usb_rw--;

	a1logd(p->log, 8, "coms_usb_transaction: returning err 0x%x and %d bytes\n",rv, xlength);

	return rv;
}


/* Our control message routine. */
/* It's basically up to the caller to set parameters that make up a valid request, */
/* as they are fed directly into the control setup. */
/* Return error icom error code */
static int icoms_usb_control_msg_dk(
	icoms *p,
	int *transferred,
	int requesttype,			/* Setup values */
	int request,
	int value,
	int index,
	unsigned char *bytes,		/* Data to read or write */
	int size, 					/* Size of data to read or write */
	int timeout
) {
	USB_DK_TRANSFER_REQUEST Request = { 0 };
	TransferResult result;
	struct usbdk_olr olr;
	int rv = ICOM_OK;
	int dirw = (requesttype & IUSB_REQ_DIR_MASK) == IUSB_REQ_HOST_TO_DEV ? 1 : 0;
	int tsize;
	unsigned char *tbuf = NULL;
	DWORD xlength;
	int retsz = 0;

	a1logd(p->log, 8, "icoms_usb_control_msg_dk: type 0x%x req 0x%x size %d\n",requesttype,request,size);

	if (transferred != NULL)
		*transferred = 0;

	/* We're guessing that the first 8 bytes of the buffer are the setup, */
	/* with anything after that being the read/write data... */

	/* Allocate buffer that includes setup */
    tsize = 8 + size;
	if ((tbuf = calloc(1, tsize)) == NULL) {
		a1loge(p->log, ICOM_SYS, "icoms_usb_control_msg_dk: calloc failed\n");
		return ICOM_SYS;
	}
	short2usb(&tbuf[6], size);

    if (dirw)
        memcpy(tbuf + 8, bytes, size);		/* Copy write data */

	/* Fill in the setup */
	tbuf[0] = requesttype;
	tbuf[1] = request;
	short2usb(&tbuf[2], value);
	short2usb(&tbuf[4], index);

	Request.EndpointAddress = 0;	/* Control pipe */
	Request.Buffer = tbuf;
	Request.BufferLength = tsize;
	Request.TransferType = ControlTransferType;

	memset(&olr, 0, sizeof(struct usbdk_olr));

	if ((olr.olaps.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL)
		return ICOM_SYS;

	add_olr(p, &olr);

	if (dirw) {
		result = UsbDk_WritePipe(p->usbd->handle, &Request, &olr.olaps);
	} else {
		result = UsbDk_ReadPipe(p->usbd->handle, &Request, &olr.olaps);
	}

	if (result == TransferFailure) {
		CloseHandle(olr.olaps.hEvent);
		return ICOM_USBW;
	}

	/* Hmm. If we don't do this, the UsbDk driver locks up on */
	/* this device, and the system needs a re-boot. */
	if (!GetOverlappedResult(p->usbd->syshandle, &olr.olaps, &xlength, TRUE)) {
		CloseHandle(olr.olaps.hEvent);
		olr.olaps.hEvent = NULL;
		rem_olr(p, &olr);
		return ICOM_USBR;
	}
	CloseHandle(olr.olaps.hEvent);
	olr.olaps.hEvent = NULL;
	rem_olr(p, &olr);

	/* xlength doesn't seem to correspond with what UsbDk did, BytesTransferred does. */
	retsz = (int)Request.Result.GenResult.BytesTransferred;

	if (!dirw && retsz > 0) {
        memcpy(bytes, tbuf + 8, retsz);		/* Copy read data */
	}

	if (transferred != NULL)
		*transferred = retsz;

	/* The requested size wasn't transferred */
	if (rv == ICOM_OK  && retsz != size)
		rv |= ICOM_SHORT;

	a1logd(p->log, 8, "icoms_usb_control_msg_dk: returning err 0x%x and %d bytes\n",rv, retsz);
	return rv;
}

/* Cancel i/o operation in another thread. */
/* Only Vista has CancelIoEx that can cancel a single operation, */
/* so we cancel the io to the end point, which will */
/* achieve what we want. */
int icoms_usb_cancel_io_dk(
	icoms *p,
	usb_cancelt *cancelt
) {
	int rv = ICOM_OK;

	amutex_lock(cancelt->cmtx);
	if (cancelt->hcancel != NULL) {
		int ep = *((unsigned char *)cancelt->hcancel);

		if (!UsbDk_AbortPipe(p->usbd->handle, (ULONG64)ep)) {
			a1logd(p->log, 1, "icoms_usb_cancel_io_dk failed\n");
			rv = ICOM_SYS;
		}
	}
	amutex_unlock(cancelt->cmtx);
	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Reset and end point data toggle to 0 */
int icoms_usb_resetep_dk(
	icoms *p,
	int ep					/* End point address */
) {
	if (!UsbDk_ResetPipe(p->usbd->handle, (ULONG64)ep)) {
		a1logd(p->log, 1, "icoms_usb_resetep_dk failed\n");
		return ICOM_SYS;
	}
	return ICOM_OK;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Clear a halt on an end point */
/* (Actually does a resetep) */
int icoms_usb_clearhalt_dk(
	icoms *p,
	int ep					/* End point address */
) {
	if (!UsbDk_ResetPipe(p->usbd->handle, (ULONG64)ep)) {
		a1logd(p->log, 1, "icoms_resetpipe/clearhalt_dk failed\n");
		return ICOM_SYS;
	}
	return ICOM_OK;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */
