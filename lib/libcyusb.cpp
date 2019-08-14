/*******************************************************************************\
 * Program Name		:	libcyusb.cpp					*
 * Author		:	V. Radhakrishnan ( rk@atr-labs.com )		*
 * License		:	LGPL Ver 2.1				        *
 * Copyright		:	Cypress Semiconductors Inc. / ATR-LABS		*
 * Date written		:	March 12, 2012					*
 * Modification Notes	:							*
 * 										*
 * This program is the main library for all cyusb applications to use.		*
 * This is a thin wrapper around libusb						*
 \*******************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <libusb-1.0/libusb.h>
#include "../include/cyusb.h"

/* Maximum length of a string read from the Configuration file (/etc/cyusb.conf) for the library. */
#define MAX_CFG_LINE_LENGTH                     (120)

/* Maximum length for a filename. */
#define MAX_FILEPATH_LENGTH			(256)

/* Maximum size of EZ-USB FX3 firmware binary. Limited by amount of RAM available. */
#define FX3_MAX_FW_SIZE				(524288)

static struct cydev 	cydev[MAXDEVICES];		/* List of devices of interest that are connected. */
static int          	nid;				/* Number of Interesting Devices. */
static libusb_device	**list;				/* libusb device list used by the cyusb library. */

/*
   struct VPD
   Used to store information about the devices of interest listed in /etc/cyusb.conf
 */
struct VPD {
	unsigned short	vid;				/* USB Vendor ID. */
	unsigned short	pid;				/* USB Product ID. */
	char		desc[MAX_STR_LEN];		/* Device description. */
};

static struct VPD	vpd[MAX_ID_PAIRS];		/* Known device database. */
static int 		maxdevices;			/* Number of devices in the vpd database. */
static unsigned int 	checksum = 0;			/* Checksum calculated on the Cypress firmware binary. */

/* The following variables are used by the cyusb_linux application. */
       char		pidfile[MAX_FILEPATH_LENGTH];	/* Full path to the PID file specified in /etc/cyusb.conf */
       char		logfile[MAX_FILEPATH_LENGTH];	/* Full path to the LOG file specified in /etc/cyusb.conf */
       int		logfd;				/* File descriptor for the LOG file. */
       int		pidfd;				/* File descriptor for the PID file. */

/* isempty:
   Check if the first L characters of the string buf are white-space characters.
 */
static bool
isempty (
		char *buf,
		int L)
{
	bool flag = true;
	int i;

	for (i = 0; i < L; ++i ) {
		if ( (buf[i] != ' ') && ( buf[i] != '\t' ) ) {
			flag = false;
			break;
		}
	}

	return flag;
}

/* parse_configfile:
   Parse the /etc/cyusb.conf file and get the list of USB devices of interest.
 */
static void
parse_configfile (
		void)
{
	FILE *inp;
	char buf[MAX_CFG_LINE_LENGTH];
	char *cp1, *cp2, *cp3;
	int i;

	inp = fopen("/etc/cyusb.conf", "r");
	if (inp == NULL)
		return;

	memset(buf,'\0',MAX_CFG_LINE_LENGTH);
	while ( fgets(buf,MAX_CFG_LINE_LENGTH,inp) ) {
		if ( buf[0] == '#' ) 			/* Any line starting with a # is a comment 	*/
			continue;
		if ( buf[0] == '\n' )
			continue;
		if ( isempty(buf,strlen(buf)) )		/* Any blank line is also ignored		*/
			continue;

		cp1 = strtok(buf," =\t\n");
		if ( !strcmp(cp1,"LogFile") ) {
			cp2 = strtok(NULL," \t\n");
			strcpy(logfile,cp2);
		}
		else if ( !strcmp(cp1,"PIDFile") ) {
			cp2 = strtok(NULL," \t\n");
			strcpy(pidfile,cp2);
		}
		else if ( !strcmp(cp1,"<VPD>") ) {
			while ( fgets(buf,MAX_CFG_LINE_LENGTH,inp) ) {
				if ( buf[0] == '#' ) 		/* Any line starting with a # is a comment 	*/
					continue;
				if ( buf[0] == '\n' )
					continue;
				if ( isempty(buf,strlen(buf)) )	/* Any blank line is also ignored		*/
					continue;
				if ( maxdevices == (MAX_ID_PAIRS - 1) )
					continue;
				cp1 = strtok(buf," \t\n");
				if ( !strcmp(cp1,"</VPD>") )
					break;
				cp2 = strtok(NULL, " \t");
				cp3 = strtok(NULL, " \t\n");

				vpd[maxdevices].vid = strtol(cp1,NULL,16);
				vpd[maxdevices].pid = strtol(cp2,NULL,16);
				strncpy(vpd[maxdevices].desc,cp3,MAX_STR_LEN);
				vpd[maxdevices].desc[MAX_STR_LEN - 1] = '\0';   /* Make sure of NULL-termination. */

				++maxdevices;
			}
		}
		else {
			printf("Error in config file /etc/cyusb.conf: %s \n",buf);
			exit(1);
		}
	}

	fclose(inp);
}

/* device_is_of_interest:
   Check whether the current USB device is among the devices of interest.
 */
static int
device_is_of_interest (
		libusb_device *d)
{
	int i;
	struct libusb_device_descriptor desc;
	int vid, pid;

	libusb_get_device_descriptor(d, &desc);
	vid = desc.idVendor;
	pid = desc.idProduct;

	for ( i = 0; i < maxdevices; ++i ) {
		if ( (vpd[i].vid == vid) && (vpd[i].pid == pid) ) {
			printf("Found device %04x %04x \n", vid, pid);
			return 1;
		}
	}
	return 0;
}

/* cyusb_getvendor:
   Get the Vendor ID for the current USB device.
 */
unsigned short
cyusb_getvendor (
		libusb_device_handle *h)
{
	struct libusb_device_descriptor d;
	libusb_device *tdev = libusb_get_device(h);
	libusb_get_device_descriptor(tdev, &d);
	return d.idVendor;
}

/* renumerate:
   Get handles to and store information about all USB devices of interest.
 */
static int
renumerate (
		void)
{
	libusb_device *dev = NULL;
	libusb_device_handle *handle = NULL;
	struct libusb_device_descriptor desc;
	int           numdev;
	int           i;
	int           r;

	numdev = libusb_get_device_list(NULL, &list);
	if ( numdev < 0 ) {
		printf("Library: Error in enumerating devices...\n");
		return -ENODEV;
	}

	nid = 0;
	for ( i = 0; i < numdev; ++i ) {
		libusb_device *tdev = list[i];
		if ( device_is_of_interest(tdev) ) {
			cydev[nid].dev = tdev;
			r = libusb_open(tdev, &cydev[nid].handle);
			if ( r ) {
				printf("Error in opening device %d\n", r);
				return -EACCES;
			}
			else
				handle = cydev[nid].handle;

			libusb_get_device_descriptor(tdev, &desc);
			cydev[nid].vid     = desc.idVendor;
			cydev[nid].pid     = desc.idProduct;
			cydev[nid].is_open = 1;
			cydev[nid].busnum  = libusb_get_bus_number(tdev);
			cydev[nid].devaddr = libusb_get_device_address(tdev);
			++nid;
		}
	}

	return nid;
}

/* cyusb_open:
   Opens handles to all USB devices of interest, and returns their count.
 */
int cyusb_open (
		void)
{
	int fd1;
	int r;

	fd1 = open("/etc/cyusb.conf", O_RDONLY);
	if ( fd1 < 0 ) {
		printf("/etc/cyusb.conf file not found. Exiting\n");
		return -ENOENT;
	}
	else {
		close(fd1);
		parse_configfile();	/* Parse the file and store information inside exported data structures */
	}

	r = libusb_init(NULL);
	if (r) {
		printf("Error in initializing libusb library...\n");
		return -EACCES;
	}

	/* Get list of USB devices of interest. */
	r = renumerate();
	return r;
}

/* cyusb_open:
   Open a handle to the USB device with specified vid/pid.
 */
int cyusb_open (
		unsigned short vid,
		unsigned short pid)
{
	int r;
	libusb_device *dev = NULL;
	libusb_device_handle *h = NULL;
	struct libusb_device_descriptor desc;

	r = libusb_init(NULL);
	if (r) {
		printf("Error in initializing libusb library...\n");
		return -EACCES;
	}

	h = libusb_open_device_with_vid_pid(NULL, vid, pid);
	if ( !h ) {
		printf("Device not found\n");
		return -ENODEV;
	}

	dev = libusb_get_device(h);
	cydev[0].dev     = dev;
	cydev[0].handle  = h;

	libusb_get_device_descriptor(dev, &desc);
	cydev[0].vid     = desc.idVendor;
	cydev[0].pid     = desc.idProduct;
	cydev[0].is_open = 1;
	cydev[0].busnum  = libusb_get_bus_number(dev);
	cydev[0].devaddr = libusb_get_device_address(dev);
	nid = 1;

	return 1;
}

/* cyusb_error:
   Print verbose information about the error returned by the cyusb API. These are essentially descriptions of
   status values defined as part of the libusb library.
 */
void
cyusb_error (
		int err)
{
	switch (err)
	{
		case -1:
			fprintf(stderr, "Input/output error\n");
			break;
		case -2:
			fprintf(stderr, "Invalid parameter\n");
			break;
		case -3:
			fprintf(stderr, "Access denied (insufficient permissions)\n");
			break;
		case -4:
			fprintf(stderr, "No such device. Disconnected...?\n");
			break;
		case -5:
			fprintf(stderr, "Entity not found\n");
			break;
		case -6:
			fprintf(stderr, "Resource busy\n");
			break;
		case -7:
			fprintf(stderr, "Operation timed out\n");
			break;
		case -8:
			fprintf(stderr, "Overflow\n");
			break;
		case -9:
			fprintf(stderr, "Pipe error\n");
			break;
		case -10:
			fprintf(stderr, "System call interrupted, ( due to signal ? )\n");
			break;
		case -11:
			fprintf(stderr, "Insufficient memory\n");
			break;
		case -12:
			fprintf(stderr, "Operation not supported/implemented\n");
			break;
		default:
			fprintf(stderr, "Unknown internal error\n");
			break;
	}
}

/* cyusb_gethandle:
   Get a handle to the USB device with specified index.
 */
libusb_device_handle *
cyusb_gethandle (
		int index)
{
	return cydev[index].handle;
}

/* cyusb_close:
   Close all device handles and de-initialize the libusb library.
 */
void
cyusb_close (
		void)
{
	int i;

	for ( i = 0; i < nid; ++i ) {
		libusb_close(cydev[i].handle);
	}

	libusb_free_device_list(list, 1);
	libusb_exit(NULL);
}


/* cyusb_download_fx2:
   Download firmware to the Cypress FX2/FX2LP device using USB vendor commands.
 */
int
cyusb_download_fx2 (
		libusb_device_handle *h,
		char *filename,
		unsigned char vendor_command)
{
	FILE *fp = NULL;
	char buf[256];
	char tbuf1[3];
	char tbuf2[5];
	char tbuf3[3];
	unsigned char reset = 0;
	int r;
	int count = 0;
	unsigned char num_bytes = 0;
	unsigned short address = 0;
	unsigned char *dbuf = NULL;
	int i;

	fp = fopen(filename, "r" );
	tbuf1[2] ='\0';
	tbuf2[4] = '\0';
	tbuf3[2] = '\0';

	/* Place the FX2/FX2LP CPU in reset, so that the vendor commands can be handled by the device. */
	reset = 1;
	r = libusb_control_transfer(h, 0x40, 0xA0, 0xE600, 0x00, &reset, 0x01, 1000);
	if ( !r ) {
		printf("Error in control_transfer\n");
		return r;
	}
	sleep(1);

	count = 0;

	while ( fgets(buf, 256, fp) != NULL ) {
		if ( buf[8] == '1' )
			break;
		strncpy(tbuf1,buf+1,2);
		num_bytes = strtoul(tbuf1,NULL,16);
		strncpy(tbuf2,buf+3,4);
		address = strtoul(tbuf2,NULL,16);
		dbuf = (unsigned char *)malloc(num_bytes);
		for ( i = 0; i < num_bytes; ++i ) {
			strncpy(tbuf3,&buf[9+i*2],2);
			dbuf[i] = strtoul(tbuf3,NULL,16);
		}

		r = libusb_control_transfer(h, 0x40, vendor_command, address, 0x00, dbuf, num_bytes, 1000);
		if ( !r ) {
			printf("Error in control_transfer\n");
			free(dbuf);
			return r;
		}
		count += num_bytes;
		free(dbuf);
	}

	printf("Total bytes downloaded = %d\n", count);
	sleep(1);

	/* Bring the CPU out of reset to run the newly loaded firmware. */
	reset = 0;
	r = libusb_control_transfer(h, 0x40, 0xA0, 0xE600, 0x00, &reset, 0x01, 1000);
	fclose(fp);
	return 0;
}

/* control_transfer:
   Internal function that issues the vendor command that incrementally loads firmware segments to the
   Cypress FX3 device RAM.
 */
static void
control_transfer (
		libusb_device_handle *h,
	       	unsigned int address,
	       	unsigned char *dbuf,
	       	int len)
{
	int j;
	int r;
	int b;
	unsigned int *pint;
	int index;

	int balance = len;
	pint = (unsigned int *)dbuf;

	index = 0;
	while ( balance > 0 ) {
		if ( balance > 4096 )
			b = 4096;
		else b = balance;
		r = libusb_control_transfer (h, 0x40, 0xA0, ( address & 0x0000ffff ), address >> 16,
			       	&dbuf[index], b, 1000);
		if ( r != b ) {
			printf("Error in control_transfer\n");
		}
		address += b ;
		balance -= b;
		index += b;
	}

	/* Update the firmware checksum as the download is being performed. */
	for ( j = 0; j < len/4; ++j )
		checksum += pint[j];
}

/* cyusb_download_fx3:
   Download a firmware binary the Cypress FX3 device RAM.
 */
int
cyusb_download_fx3 (
		libusb_device_handle *h,
	       	const char *filename)
{
	int fd;
	unsigned char buf[FX3_MAX_FW_SIZE];
	int nbr;
	int dlen;
	int count;
	unsigned int *pdbuf = NULL;
	unsigned int address;
	unsigned int *pint;
	unsigned int program_entry;
	int r;

	fd = open(filename, O_RDONLY);
	if ( fd < 0 ) {
		printf("File not found\n");
		return -ENOENT;
	}
	else
		printf("File successfully opened\n");

	count = 0;
	checksum = 0;
	nbr = read(fd, buf, 2);		/* Read first 2 bytes, must be equal to 'CY'	*/
	if ( strncmp((char *)buf,"CY",2) ) {
		printf("Image does not have 'CY' at start. aborting\n");
		return -EINVAL;
	}
	nbr = read(fd, buf, 1);		/* Read 1 byte. bImageCTL	*/
	if ( buf[0] & 0x01 ) {
		printf("Image does not contain executable code\n");
		return -EINVAL;
	}
	nbr = read(fd, buf, 1);		/* Read 1 byte. bImageType	*/
	if ( !(buf[0] == 0xB0) ) {
		printf("Not a normal FW binary with checksum\n");
		return -EINVAL;
	}

	while (1) {
		nbr = read(fd, buf, 4);	/* Read Length of section 1,2,3, ...	*/
		pdbuf = (unsigned int *)buf;
		dlen = *pdbuf;
		nbr = read(fd,buf,4);	/* Read Address of section 1,2,3,...	*/
		pint = (unsigned int *)buf;
		address = *pint;
		if ( dlen != 0 ) {
			nbr = read(fd, buf, dlen*4);	/* Read data bytes	*/
			control_transfer(h, address, buf, dlen*4);
		}
		else {
			program_entry = address;
			break;
		}
	}

	nbr = read(fd, buf, 4);			/* Read checksum	*/
	pdbuf = (unsigned int *)buf;
	if ( *pdbuf != checksum ) {
		printf("Error in checksum\n");
		return -EINVAL;
	}

	sleep(1);
	r = libusb_control_transfer(h, 0x40, 0xA0, (program_entry & 0x0000ffff ) , program_entry >> 16, NULL, 0, 1000);
	if ( r ) {
		printf("Ignored error in control_transfer: %d\n", r);
	}

	close(fd);
	return 0;
}

/*[]*/

