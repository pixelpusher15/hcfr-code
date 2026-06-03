
/* General USB I/O support for MSWin. */
/* This is a shim to allow usbio_w0 and usbio_dk to both be used. */
/* This file is conditionaly #included into usbio.c */
 
/* 
 * Argyll Color Management System
 *
 * Author: Graeme W. Gill
 * Date:   2023/11/16
 *
 * Copyright 2006 - 2023 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/* Note that one of EN_LIBUSB0 or EN_USBDK both will be #defined... */ 

/* Add paths to USB connected device */
int usb_get_paths(
icompaths *p 
) {
	int w0 = 0, dk;

#ifdef EN_LIBUSB0
	w0 = usb_get_paths_w0(p);
# ifndef EN_USBDK
	return w0;
# endif
#endif

#ifdef EN_USBDK
	dk = usb_get_paths_dk(p);
# ifdef EN_LIBUSB0
	if (dk != 0)
		return w0;
# endif
	return dk;
#endif
}

/* Copy usb_idevice contents from icompaths to icom */
int usb_copy_usb_idevice(icoms *d, icompath *s) {
	if (s->usbd == NULL) { 
		d->usbd = NULL;
		return ICOM_OK;
	}
#ifdef EN_USBDK
	if (s->usbd->is_dk)
		return usb_copy_usb_idevice_dk(d, s);
#endif
#ifdef EN_LIBUSB0
	return usb_copy_usb_idevice_w0(d, s);
#endif
}

/* Cleanup and then free a usb dev entry */
void usb_del_usb_idevice(struct usb_idevice *usbd) {
	if (usbd == NULL)
		return;
#ifdef EN_USBDK
	if (usbd->is_dk) {
		usb_del_usb_idevice_dk(usbd);
		return;
	}
#endif
#ifdef EN_LIBUSB0
	usb_del_usb_idevice_w0(usbd);
#endif
}

/* Cleanup any USB specific icoms state */
void usb_del_usb(icoms *p) {
	if (p->usbd == NULL)
		return;
#ifdef EN_USBDK
	if (p->usbd->is_dk) {
		usb_del_usb_dk(p);
		return;
	}
#endif
#ifdef EN_LIBUSB0
	usb_del_usb_w0(p);
#endif
}

/* Close an open USB port */
void usb_close_port(icoms *p) {
	if (p->usbd == NULL)
		return;
#ifdef EN_USBDK
	if (p->usbd->is_dk) {
		usb_close_port_dk(p);
		return;
	}
#endif
#ifdef EN_LIBUSB0
	usb_close_port_w0(p);
#endif
}

/* Open a USB port for all our uses. */
static int usb_open_port(
icoms *p,
int    config,		/* Configuration number */
int    wr_ep,		/* Write end point */
int    rd_ep,		/* Read end point */
icomuflags usbflags,/* Any special handling flags */
int retries,		/* > 0 if we should retry set_configuration (100msec) */ 
char **pnames		/* List of process names to try and kill before opening */
) {
#ifdef EN_USBDK
	if (p->usbd->is_dk)
		return usb_open_port_dk(p, config, wr_ep, rd_ep, usbflags, retries, pnames);
#endif
#ifdef EN_LIBUSB0
	return usb_open_port_w0(p, config, wr_ep, rd_ep, usbflags, retries, pnames);
#endif
}

/*  -------------------------------------------------------------- */

/* Universal (non-control pipe) USB transfer function, used for rd/wr. */
static int icoms_usb_transaction(
	icoms *p,
	usb_cancelt *cancelt,		/* if !NULL, allows signalling another thread that op has started */
	int *transferred,
	icom_usb_trantype ttype,	/* transfer type */
	unsigned char endpoint,		/* 0x80 for control write, 0x00 for control read */
	unsigned char *buffer,
	int length,					/* Length to write/expected length to read */
	unsigned int timeout		/* In msec */
) {
#ifdef EN_USBDK
	if (p->usbd->is_dk)
		return icoms_usb_transaction_dk(p, cancelt, transferred, ttype, endpoint, buffer, length, timeout);
#endif
#ifdef EN_LIBUSB0
	return icoms_usb_transaction_w0(p, cancelt, transferred, ttype, endpoint, buffer, length, timeout);
#endif
}

/* Control message routine */
static int icoms_usb_control_msg(
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
#ifdef EN_USBDK
	if (p->usbd->is_dk)
		return icoms_usb_control_msg_dk(p, transferred, requesttype, request, value, index, bytes, size, timeout);
#endif
#ifdef EN_LIBUSB0
	return icoms_usb_control_msg_w0(p, transferred, requesttype, request, value, index, bytes, size, timeout);
#endif
}

/* Cancel i/o operation in another thread. */
int icoms_usb_cancel_io(
	icoms *p,
	usb_cancelt *cancelt
) {
#ifdef EN_USBDK
	if (p->usbd->is_dk)
		return icoms_usb_cancel_io_dk(p, cancelt);
#endif
#ifdef EN_LIBUSB0
	return icoms_usb_cancel_io_w0(p, cancelt);
#endif
}

/* Reset and end point data toggle to 0 */
int icoms_usb_resetep(
	icoms *p,
	int ep					/* End point address */
) {
#ifdef EN_USBDK
	if (p->usbd->is_dk)
		return icoms_usb_resetep_dk(p, ep);
#endif
#ifdef EN_LIBUSB0
	return icoms_usb_resetep_w0(p, ep);
#endif
}

/* Clear a halt on an end point */
int icoms_usb_clearhalt(
	icoms *p,
	int ep					/* End point address */
) {
#ifdef EN_USBDK
	if (p->usbd->is_dk)
		return icoms_usb_clearhalt_dk(p, ep);
#endif
#ifdef EN_LIBUSB0
	return icoms_usb_clearhalt_w0(p, ep);
#endif
}

