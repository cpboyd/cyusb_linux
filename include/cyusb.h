#ifndef __CYUSB_H
#define __CYUSB_H

/*********************************************************************************\
 * This is the main header file for the cyusb suite for Linux/Mac, called cyusb.h *
 *                                                                                *
 * Author              :        V. Radhakrishnan ( rk@atr-labs.com )              *
 * License             :        LGPL Ver 2.1                                      *
 * Copyright           :        Cypress Semiconductors Inc. / ATR-LABS            *
 * Date written        :        March 12, 2012                                    *
 * Modification Notes  :                                                          *
 *    1. Cypress Semiconductor, January 23, 2013                                  *
 *       Added function documentation.                                            *
 *       Added new constant to specify number of device ID entries.               *
 *                                                                                *
 \********************************************************************************/

#include <libusb-1.0/libusb.h>

/* This is the maximum number of 'devices of interest' we are willing to store as default. */
/* These are the maximum number of devices we will communicate with simultaneously */
#define MAXDEVICES        10

/* This is the maximum number of VID/PID pairs that this library will consider. This limits
   the number of valid VID/PID entries in the configuration file.
 */
#define MAX_ID_PAIRS    100

/* This is the maximum length for the description string for a device in the configuration
   file. If the actual string in the file is longer, only the first MAX_STR_LEN characters
   will be considered.
 */
#define MAX_STR_LEN     30

struct cydev {
    libusb_device *dev;          /* as above ... */
    libusb_device_handle *handle;       /* as above ... */
    unsigned short vid;         /* Vendor ID */
    unsigned short pid;         /* Product ID */
    unsigned char is_open;      /* When device is opened, val = 1 */
    unsigned char busnum;       /* The bus number of this device */
    unsigned char devaddr;      /* The device address*/
    unsigned char filler;       /* Padding to make struct = 16 bytes */
};

/* Function prototypes */

/*******************************************************************************************
  Prototype    : int cyusb_error(int err);
  Description  : Print out a verbose message corresponding to an error code, to the stderr
                 stream.
  Parameters   :
                 int err : Error code
  Return Value : none
 *******************************************************************************************/
extern void cyusb_error(int err);

/*******************************************************************************************
  Prototype    : int cyusb_open(void);
  Description  : This initializes the underlying libusb library, populates the cydev[]
                 array, and returns the number of devices of interest detected. A
                 'device of interest' is a device which appears in the /etc/cyusb.conf file.
  Parameters   : None
  Return Value : Returns an integer, equal to number of devices of interest detected.
 *******************************************************************************************/
extern int cyusb_open(void);

/*******************************************************************************************
  Prototype    : int cyusb_open(unsigned short vid, unsigned short pid);
  Description  : This is an overloaded function that populates the cydev[] array with
                 just one device that matches the provided vendor ID and Product ID.
                 This function is only useful if you know in advance that there is only
                 one device with the given VID and PID attached to the host system.
  Parameters   :
                 unsigned short vid : Vendor ID
                 unsigned short pid : Product ID
  Return Value : Returns 1 if a device of interest exists, else returns 0.
 *******************************************************************************************/
extern int cyusb_open(unsigned short vid, unsigned short pid);

/*******************************************************************************************
  Prototype    : libusb_device_handle * cyusb_gethandle(int index);
  Description  : This function returns a libusb_device_handle given an index from the cydev[] array.
  Parameters   :
                 int index : Equal to the index in the cydev[] array that gets populated
                             during the cyusb_open() call described above.
  Return Value : Returns the pointer to a struct of type libusb_device_handle.
 *******************************************************************************************/
extern libusb_device_handle * cyusb_gethandle(int index);

/*******************************************************************************************
  Prototype    : unsigned short cyusb_getvendor(libusb_device_handle *);
  Description  : This function returns a 16-bit value corresponding to the vendor ID given
                 a device's handle.
  Parameters   :
                 libusb_device_handle *handle : Pointer to a struct of type libusb_device_handle.
  Return Value : Returns the 16-bit unique vendor ID of the given device.
 *******************************************************************************************/
extern unsigned short cyusb_getvendor(libusb_device_handle *);

/*******************************************************************************************
  Prototype    : void cyusb_close(void);
  Description  : This function closes the libusb library and releases memory allocated to cydev[].
  Parameters   : none.
  Return Value : none.
 *******************************************************************************************/
extern void cyusb_close(void);

/****************************************************************************************
  Prototype    : void cyusb_download_fx2(libusb_device_handle *h, char *filename,
                     unsigned char vendor_command);
  Description  : Performs firmware download on FX2.
  Parameters   :
                 libusb_device_handle *h              : Device handle
                 char * filename              : Path where the firmware file is stored
                 unsigned char vendor_command : Vendor command that needs to be passed during download
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR.
 ****************************************************************************************/
extern int cyusb_download_fx2(libusb_device_handle *h, char *filename, unsigned char vendor_command);


/****************************************************************************************
  Prototype    : void cyusb_download_fx3(libusb_device_handle *h, char *filename);
  Description  : Performs firmware download on FX3.
  Parameters   :
                 libusb_device_handle *h : Device handle
                 char *filename  : Path where the firmware file is stored
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR.
 ***************************************************************************************/
extern int cyusb_download_fx3(libusb_device_handle *h, char *filename);

#endif /* __CYUSB_H */
