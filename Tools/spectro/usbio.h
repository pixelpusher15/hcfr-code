
#ifndef USBIO_H

 /* General USB I/O support */

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

#ifdef ENABLE_USB

#ifdef __cplusplus
	extern "C" {
#endif

/* Standard USB protocol defines */
# include "iusb.h"

/* Opaque structure to hide implementation details in icoms */ 

# ifdef NT


#ifdef EN_USBDK
/* A record of an outstanding overlap object, to allow cleanup. */
struct usbdk_olr {
	struct usbdk_olr **last;
	struct usbdk_olr *next;
	OVERLAPPED olaps;
};
#endif

/* MSWindows USB context */
struct usb_idevice {
	/* icompath stuff: */
#ifdef EN_USBDK
	int is_dk;				/* nz if this device was opened by usbio_dk */
	USB_DK_DEVICE_ID ID;
#endif
#ifdef EN_LIBUSB0
	char *dpath;			/* Device path */
#endif
	unsigned int iserialno;
	char *SerialNumber;		/* If not-NULL, USB serial number string */
	int nconfig;			/* Number of configurations */
	int config;				/* This config (always 1) */
	int nifce;				/* Number of interfaces in this config */
	usb_ep ep[32];			/* Information about each end point for general usb i/o */

	/* Stuff setup when device is open: */
	HANDLE handle;			/* Device handle */
#ifdef EN_USBDK
	HANDLE syshandle;		/* System handle */
	CRITICAL_SECTION lock;	/* protect olr manipulation */
	struct usbdk_olr *olr;
#endif
};

# endif	/* NT */

# if defined(UNIX_APPLE)

/* OS X structure version wrangling */

/* usb_device_t - for communicating with the device.  */

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1070
# define usb_device_t    IOUSBDeviceInterface500
# define DeviceInterfaceID kIOUSBDeviceInterfaceID500
# define DeviceVersion 500
#elif MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
# define usb_device_t    IOUSBDeviceInterface320
# define DeviceInterfaceID kIOUSBDeviceInterfaceID320
# define DeviceVersion 320
#elif MAC_OS_X_VERSION_MIN_REQUIRED >= 1050
# define usb_device_t    IOUSBDeviceInterface300
# define DeviceInterfaceID kIOUSBDeviceInterfaceID300
# define DeviceVersion 300
#elif MAC_OS_X_VERSION_MIN_REQUIRED >= 1040
# define usb_device_t    IOUSBDeviceInterface245
# define DeviceInterfaceID kIOUSBDeviceInterfaceID245
# define DeviceVersion 245
#elif MAC_OS_X_VERSION_MIN_REQUIRED >= 1020
# define usb_device_t    IOUSBDeviceInterface197
# define DeviceInterfaceID kIOUSBDeviceInterfaceID197
# define DeviceVersion 197
#elif MAC_OS_X_VERSION_MIN_REQUIRED >= 1010
# define usb_device_t    IOUSBDeviceInterface182
# define DeviceInterfaceID kIOUSBDeviceInterfaceID182
# define DeviceVersion 182
#else
# define usb_device_t    IOUSBDeviceInterface
# define DeviceInterfaceID kIOUSBDeviceInterfaceID
# define DeviceVersion 100
#endif

/* usb_interface_t - for communicating with an interface in the device */
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1070
# define usb_interface_t IOUSBInterfaceInterface500
# define InterfaceInterfaceID kIOUSBInterfaceInterfaceID500
# define InterfaceVersion 500
#elif MAC_OS_X_VERSION_MIN_REQUIRED >= 1050
# define usb_interface_t IOUSBInterfaceInterface300
# define InterfaceInterfaceID kIOUSBInterfaceInterfaceID300
# define InterfaceVersion 300
#elif MAC_OS_X_VERSION_MIN_REQUIRED >= 1040
# define usb_interface_t IOUSBInterfaceInterface220
# define InterfaceInterfaceID kIOUSBInterfaceInterfaceID220
# define InterfaceVersion 220
#elif MAC_OS_X_VERSION_MIN_REQUIRED >= 1020
# define usb_interface_t IOUSBInterfaceInterface190
# define InterfaceInterfaceID kIOUSBInterfaceInterfaceID190
# define InterfaceVersion 190
#elif MAC_OS_X_VERSION_MIN_REQUIRED >= 1010
# define usb_interface_t IOUSBInterfaceInterface182
# define InterfaceInterfaceID kIOUSBInterfaceInterfaceID182
# define InterfaceVersion 182
#else
# define usb_interface_t IOUSBInterfaceInterface
# define InterfaceInterfaceID kIOUSBInterfaceInterfaceID
# define InterfaceVersion 100
#endif


/* OS X native USB context */
struct usb_idevice {
	/* icompath stuff: */
	int lid;					/* Location ID */
	io_object_t ioob;			/* USB io registry object */
	char *SerialNumber;			/* If not-NULL, USB serial number string */
	int nconfig;				/* Number of configurations */
	int config;					/* This config (always 1) */
	int nifce;					/* Number of interfaces */
	/* Stuff setup when device is open: */
	usb_device_t **device;		/* OS X USB device we've opened */
	usb_interface_t **interfaces[32];	/* nifce interfaces */
	CFRunLoopSourceRef cfsources[32];	/* Corresponding event sources */
	pthread_t thread;			/* RunLoop thread */
	pthread_mutex_t lock;       /* Protect cfrunloop and cond */
	pthread_cond_t cond;        /* Signal from thread that it's started */
	IOReturn thrv;				/* Thread return value */
	CFRunLoopRef cfrunloop;     /* RunLoop */
	CFRunLoopSourceRef cfsource;/* Device event sources */
};
# endif	/* OS X */

# if defined(UNIX_X11)

#  if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)

/* BSD USB context */
struct usb_idevice {
	/* icompath stuff: */
	char *dpath;			/* Device path */
	char *SerialNumber;			/* If not-NULL, USB serial number string */
	int nconfig;			/* Number of configurations */
	int config;				/* This config (always 1) */
	int nifce;				/* Number of interfaces */
	usb_ep ep[32];			/* Information about each end point for general usb i/o */
	/* Stuff setup when device is open: */
	int fd;					/* Device file descriptor */
};

#  else

/* Linux USB context */
struct usb_idevice {
	/* icompath stuff: */
	char *dpath;			/* Device path */
	unsigned int iserialno;
	char *SerialNumber;		/* If not-NULL, USB serial number string */
	int nconfig;			/* Number of configurations */
	int config;				/* This config (always 1) */
	int nifce;				/* Number of interfaces */
	usb_ep ep[32];			/* Information about each end point for general usb i/o */
	/* Stuff setup when device is open: */
	int fd;					/* Device file descriptor */
	pthread_t thread;		/* Reaper thread */
	volatile int shutdown;	/* Flag to tell reaper that we're closing the fd */
	int sd_pipe[2];			/* pipe to signal sutdown */

								/* These are simply to deal with the device going away: */
	volatile int running;		/* Reaper thread is running. Set to 0 on reap failure */
	pthread_mutex_t lock;		/* Protect reqs list */
	struct _usbio_req *reqs;	/* linked list of current reqs */
};

#  endif /* Linux */

# endif	/* UNIX */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Transfer types for communicating to usb implementation */
typedef enum {
	icom_usb_trantype_command    = 0,
	icom_usb_trantype_interrutpt = 1,
	icom_usb_trantype_bulk       = 2
} icom_usb_trantype;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Cancelation token. */
/* Note that reinit & usb_transaction's must balance! */
struct _usb_cancelt {
	amutex cmtx;
	int state;			/* 0 = init, 1 = pending, 2 = complete */
	amutex condx;		/* Wait for state 0->1 sync. mutex */
	void *hcancel;		/* Pointer to implementation cancel handle */
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* These routines suplement the class code in icoms_nt.c and icoms_ux.c */

/* Add paths to USB connected instruments, to the existing */
/* icompath paths in the icoms structure. */
/* return icom error */
int usb_get_paths(struct _icompaths *p);
void usb_close_port(icoms *p);

/* Set the USB specific icoms methods */
void usb_set_usb_methods(icoms *p);

/* Copy usb_idevice contents from icompaths to icom */
/* return icom error */
int usb_copy_usb_idevice(icoms *d, icompath *s);

/* Cleanup and then free a usb_idevice */
void usb_del_usb_idevice(struct usb_idevice *dev);

/* Cleanup any USB specific icoms info */
void usb_del_usb(icoms *p);

/* Install the cleanup signal handlers */
/* (used inside usb_open_port(), hid_open_port() */
void usb_install_signal_handlers(icoms *p);

/* Delete an icoms from our static signal cleanup list */
/* (used inside usb_close_port(), hid_close_port() */
void usb_delete_from_cleanup_list(icoms *p);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Utility code */

unsigned int usb2uint(unsigned char *buf);
unsigned int usb2ushort(unsigned char *buf);
void int2usb(unsigned char *buf, int inv);
void short2usb(unsigned char *buf, int inv);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */


#ifdef __cplusplus
	}
#endif

#endif /* ENABLE_USB */

#define USBIO_H
#endif /* USBIO_H */
