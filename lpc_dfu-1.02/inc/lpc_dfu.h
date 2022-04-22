/*
 * $Id: lpc_dfu.h,v 1.5 2013/09/12 10:56:18 claudio Exp $
 */

#ifndef _LPC_DFU_H_INC
#define _LPC_DFU_H_INC

#include <stdio.h>
#include <stdint.h>
#include "libusb.h"
#include "errorcodes.h"

extern int lpc_dfu_prepare(struct libusb_device_handle *dev, int enable_debug, const char *filename);
extern int lpc_dfu_exit(struct libusb_device_handle *dev);
extern int lpc_dfu_download(struct libusb_device_handle *dev, uint32_t start_address, const char *filename, unsigned long fsize);
extern int lpc_dfu_upload(struct libusb_device_handle *dev, uint32_t start_address, const char *filename, unsigned long fsize);
extern int lpc_dfu_erase(struct libusb_device_handle *dev, uint32_t start_address, uint32_t esize);
extern int lpc_dfu_erase_all(struct libusb_device_handle *dev);
extern int lpc_dfu_reset(struct libusb_device_handle *dev);
extern int lpc_dfu_jumptoapp(struct libusb_device_handle *dev);

#endif
