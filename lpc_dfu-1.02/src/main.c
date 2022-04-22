/*
 * $Id: main.c,v 1.12 2013/11/18 15:13:05 claudio Exp $
 *
 * (C) 2013 Claudio Lanconelli <lancos@libero.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "portable.h"
#include "lpc_dfu.h"
#include "mylog.h"

#define VERSIONSTR	"v1.02"

#define NXP_LPC_VID		0x1fc9
#define NXP_LPC_PID		0x000c

static off_t file_size(const char *fname);
static bool check_valid_file(const char *fname);

FILE *mylog_fh;
int mylog_level;
unsigned int mylog_mask;

/**
static void probe_devices(libusb_context *ctx)
{
	libusb_device **list;
	ssize_t num_devs;
	ssize_t i;

	num_devs = libusb_get_device_list(ctx, &list);
	for (i = 0; i < num_devs; ++i)
	{
		struct libusb_device_descriptor desc;
		struct libusb_device *dev = list[i];

		if (libusb_get_device_descriptor(dev, &desc) == OK)
		{
				struct libusb_config_descriptor *cfg;
				int rv;

				rv = libusb_get_active_config_descriptor(dev, &cfg);
				if (rv == OK)
				{
					//...
					libusb_free_config_descriptor(cfg);
				}
		}
	}
	libusb_free_device_list(list, 0);
}
**/

#define DEFAULT_LOGNAME		"lpcdfu_log.txt"
#define DEFAULT_CMDLOGNAME	"command_log.txt"
#define DEFAULT_ADDRESS		0x80000000UL
#define READBACK_FNAME		"readback.bin"

static void print_help(void)
{
	fprintf(stderr,
			"Usage: lpc_dfu [options] ...\n"
			"  -h\t\t\tPrint this help message and exit\n"
			"  -V\t\t\tPrint the version number and exit\n"
			"  -d <level>\t\tDebug level [0-9]. Default 2\n"
			"  -x <level>\t\tlibusb Debug level. Default 2\n"
			"  -e\t\t\tErase region of flash, use -A to set start address and -S or -D to set the length\n"
			"  -E\t\t\tErase ALL flash\n"
			"  -D <filename>\t\tDownload and program flash with file content\n"
			"  -U \t\t\tUpload flash content to %s file, use -S or -D to set the length\n"
			"  -L <filename>\t\tLog file (default %s)\n"
			"  -C <filename>\t\tCommand log file (default %s)\n"
			"  -S <size>\t\tLength of bytes to transfer (if omitted is file size)\n"
			"  -A <address>\t\tAddress of flash location to start transfer (default 0x%08lx)\n",
			READBACK_FNAME, DEFAULT_LOGNAME, DEFAULT_CMDLOGNAME, DEFAULT_ADDRESS);
}

static void print_version(void)
{
	printf("lpc_dfu %s (%s %s)\nCopyright 2013 by Claudio Lanconelli\n"
			"based on dfu-util\n", VERSIONSTR, __DATE__, __TIME__);
}

int main(int argc, char *argv[])
{
	int rv = OK;
	int retry, c;
	struct libusb_device_handle *dev = NULL;
	int do_erase, do_eraseall, do_prog, do_readback;
	int dbg_level = 2, lusb_dbg_level = 2;
	unsigned long fsize = 0;
	unsigned long start_addr = DEFAULT_ADDRESS;
	const char *fname = NULL;
	const char *logfname = DEFAULT_LOGNAME;
	const char *cmdlogfname = DEFAULT_CMDLOGNAME;

	do_erase = do_eraseall = do_prog = do_readback = 0;

	c = 0;
	while (c != -1)
	{
		c = getopt(argc, argv, "hVEUeD:S:A:L:C:d:x:");
		switch (c)
		{
		case -1:
			break;
		case 'e':
			do_erase = 1;
			break;
		case 'E':
			do_eraseall = 1;
			break;
		case 'D':
			fname = optarg;
			do_prog = 1;
			break;
		case 'U':
			do_readback = 1;
			break;
		case 'A':
			start_addr = strtoul(optarg, NULL, 0);
			break;
		case 'S':
			fsize = strtoul(optarg, NULL, 0);
			break;
		case 'L':
			logfname = optarg;
			break;
		case 'C':
			cmdlogfname = optarg;
			break;
		case 'd':
			dbg_level = atoi(optarg);
			break;
		case 'x':
			lusb_dbg_level = atoi(optarg);
			break;
		case 'V':
			print_version();
			exit(0);
			break;
		case 'h':
			print_help();
			exit(0);
			break;
		default:
			print_help();
			exit(1);
			break;
		}
	}

	//Check valid option combination
	if (fname != NULL && !check_valid_file(fname))
	{
		fprintf(stderr, "Error: File %s not found\n\n", fname);
		print_help();
		exit(1);
	}
	if (!do_erase && !do_eraseall && !do_prog && !do_readback)
	{
		fprintf(stderr, "Error: You need to specify at least one of -e -E -D -U\n\n");
		print_help();
		exit(1);
	}
	if (do_erase && fsize == 0 && !check_valid_file(fname))
	{
		fprintf(stderr, "Error: -e need a valid size with -S or -D, in the latter case the erase size is the same of the file size to download\n\n");
		print_help();
		exit(1);
	}
	if (do_readback && fsize == 0 && !check_valid_file(fname))
	{
		fprintf(stderr, "Error: -U need a valid size with -S or -D, in the latter case the readback size is the same of the file size to download\n\n");
		print_help();
		exit(1);
	}
	if (do_erase && do_eraseall)
	{
		fprintf(stderr, "Error: You can't set both -e and -E\n\n");
		print_help();
		exit(1);
	}
	if (do_prog && !check_valid_file(fname))
	{
		fprintf(stderr, "Error: -D need a valid file name\n\n");
		print_help();
		exit(1);
	}

	mylog_fh = fopen(logfname, "w");
	if (mylog_fh == NULL)
		mylog_fh = stdout;

	MYLOG_SETMASK(MYLOG_MASK_ENABLEALL);
	MYLOG_SETLEVEL(dbg_level);

	if (fsize == 0 && fname != NULL)
	{
		fsize = file_size(fname);
		MY_LOG(4, MYLOG_MASK_MAIN, "File size: %lu\n", fsize);
	}

	MY_LOG(4, MYLOG_MASK_MAIN, "Init LIBUSB\n");

	rv = libusb_init(NULL);
	if (rv != OK)
	{
		fprintf(stderr, "failed to initialise libusb (%d)\n", rv);
		exit(1);
	}

	MY_LOG(4, MYLOG_MASK_MAIN, "Set libusb debug level to %d\n", lusb_dbg_level);
	libusb_set_debug(NULL, lusb_dbg_level);

	MY_LOG(4, MYLOG_MASK_MAIN, "Open Device with VID:PID %04x:%04x\n", NXP_LPC_VID, NXP_LPC_PID);

	for (retry = 0; retry < 4; retry++)
	{
		dev = libusb_open_device_with_vid_pid(NULL, NXP_LPC_VID, NXP_LPC_PID);
		if (dev == NULL)
			milli_sleep(500);
		else
			break;
	}
	if (dev == NULL)
	{
		fprintf(stderr, "failed to open usb device\n");
		libusb_exit(NULL);
		exit(1);
	}

	rv = libusb_kernel_driver_active(dev, 0);
	if (rv == LIBUSB_ERROR_NOT_SUPPORTED)
	{
		MY_LOG(3, MYLOG_MASK_MAIN, "libusb_kernel_driver_active() unsupported\n");
	}
	else
	if (rv == 1)
	{
		rv = libusb_detach_kernel_driver(dev, 0);
		if (rv)
		{
			fprintf(stderr, "Could not remove the kernel driver (%d)\n", rv);
			libusb_close(dev);
			libusb_exit(NULL);
			exit(1);
		}
	}

	MY_LOG(4, MYLOG_MASK_MAIN, "Claim interface\n");

	rv = libusb_claim_interface(dev, 0);
	if (rv)
	{
		fprintf(stderr, "Could not claim USB Dev interface (%d)\n", rv);
		libusb_close(dev);
		libusb_exit(NULL);
		exit(1);
	}
	if (rv == OK)
	{
		rv = lpc_dfu_prepare(dev, 1, cmdlogfname);
		MY_LOG(3, MYLOG_MASK_MAIN, "PREPARE Done. Result = %d\n", rv);
	}
	if (rv == OK && do_eraseall)
	{
		rv = lpc_dfu_erase_all(dev);
		MY_LOG(3, MYLOG_MASK_MAIN, "ERASE-ALL Done. Result = %d\n", rv);
	}
	if (rv == OK && do_erase)
	{
		rv = lpc_dfu_erase(dev, start_addr, fsize);
		MY_LOG(3, MYLOG_MASK_MAIN, "ERASE-REGION Done. Result = %d\n", rv);
	}
	if (rv == OK && do_prog)
	{
		rv = lpc_dfu_download(dev, start_addr, fname, fsize);
		MY_LOG(3, MYLOG_MASK_MAIN, "PROGRAM Done. Result = %d\n", rv);
	}
	if (rv == OK && do_readback)
	{
		rv = lpc_dfu_upload(dev, start_addr, "readback.bin", fsize);
		MY_LOG(3, MYLOG_MASK_MAIN, "READBACK Done. Result = %d\n", rv);
	}
	MY_LOG(3, MYLOG_MASK_MAIN, "EXIT Result=%d\n", rv);

	lpc_dfu_exit(dev);
	libusb_close(dev);
	libusb_exit(NULL);

	return rv;
}

static off_t file_size(const char *fname)
{
	struct stat st;

	if (!stat(fname, &st))
		return st.st_size;
	else
		return 0;
}

static bool check_valid_file(const char *fname)
{
	bool ret = false;

	if (fname != NULL)
	{
		FILE *fh;

		fh = fopen(fname, "rb");
		if (fh != NULL)
		{
			fclose(fh);
			ret = true;
		}
	}

	return ret;
}
