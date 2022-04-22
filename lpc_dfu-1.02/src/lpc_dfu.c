/*
 * $Id: lpc_dfu.c,v 1.13 2013/11/18 15:13:05 claudio Exp $
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

#include <dfu.h>
//#include <errno.h>
#include <errorcodes.h>
#include <libusb.h>
#include <lpc_dfu_api.h>
#include <mylog.h>
#include <portable.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
//#include <stdlib.h>
#include <string.h>
#include <time.h>
//#include <unistd.h>
#include "lpc_dfu.h"

#define DFU_BUFSIZE		2048
#define MSG_BUFSIZE		8192

static void le_uint32_to_ptr(uint8_t *p, uint32_t val);
static uint32_t le_ptr_to_uint32(const uint8_t *p);
//static void be_uint32_to_ptr(uint8_t *p, uint32_t val);
//static uint32_t be_ptr_to_uint32(const uint8_t *p);
static const char *dfuop_status_to_string(DFU_OPSTS_T sts);
static void print_dot(int loop);
static void print_hash(int loop);

static int lpc_dfu_get_status(struct libusb_device_handle *dev, int expected_state);
static int lpc_senddata(struct libusb_device_handle *dev, uint8_t *buf, unsigned int len);
static int lpc_sendcommand(struct libusb_device_handle *dev, uint32_t cmd, uint32_t addr, uint32_t siz);
static int lpc_getopstate(struct libusb_device_handle *dev, uint32_t *pcmd, uint32_t *pstatus, bool *phave_dbgstr);
static int wait_dfuop_status(struct libusb_device_handle *dev, DFU_OPSTS_T dfu_opsts, DFU_HOSTCMD_T dfu_cmd,
								int ms_to_sleep, int timeout, void (*feedback)(int loop));
static int check_dfu_idlestate(struct libusb_device_handle *dev);

static uint8_t dfu_buf[DFU_BUFSIZE];
//static unsigned long flash_erase_size = 4096;
static unsigned long flash_program_size = 4096;

static FILE *fhlog = NULL;

//Program flash from 'start_address', download the file 'filename', file size (in bytes) 'fsize'
// print a 'o' char to output any sizeof(dfu_buf) bytes programmed
int lpc_dfu_download(struct libusb_device_handle *dev, uint32_t start_address, const char *filename, unsigned long fsize)
{
	int ret = OK;
	unsigned long tot_iter;
	FILE *fh = NULL;

	if (dev == NULL || filename == NULL || fsize == 0)
		return ERRCODE(ERR_BADPARAM);

	if ((start_address % flash_program_size) != 0)
	{
		MY_LOG(0, MYLOG_MASK_LPCDFU, "Start address (0x%08lx) must be 0x%08lx aligned!\n",
				(unsigned long)start_address, (unsigned long)flash_program_size);
		return ERRCODE(ERR_BADPARAM);
	}

	if (mylog_fh != stdout)
		printf("PROGRAM - File Name: %s - Start: 0x%08lx - Size: %lu\n",
				filename, (unsigned long)start_address, (unsigned long)fsize);

	tot_iter = (fsize + sizeof(dfu_buf) - 1) / sizeof(dfu_buf);
	MY_LOG(4, MYLOG_MASK_LPCDFU, "PROGRAM - File Name: %s - Start: 0x%08lx - Size: %lu - Tot iterations: %lu\n",
			filename, (unsigned long)start_address, (unsigned long)fsize, (unsigned long)tot_iter);

	fh = fopen(filename, "rb");
	if (fh == NULL)
	{
		ret = ERRCODE(ERR_FILEOPEN);
	}
	else
	{
		if (ret == OK)
			ret = check_dfu_idlestate(dev);

		if (ret == OK)
			ret = lpc_sendcommand(dev, DFU_HOSTCMD_PROGRAM, start_address, fsize);

		if (ret == OK)
		{
			unsigned long cursize;
			unsigned long iteration;

			cursize = 0;
			iteration = 0;

			while (ret == OK && !feof(fh) && cursize < fsize)
			{
				ret = wait_dfuop_status(dev, DFU_OPSTS_PROG_STREAM, DFU_HOSTCMD_PROGRAM, 10, 90, NULL);
				if (ret == OK)
				{
					size_t n;

					n = fread(dfu_buf, 1, sizeof(dfu_buf), fh);
					if (ferror(fh))
					{
						perror("Error during file read");
						ret = ERRCODE(ERR_FILEREAD);
					}
					else
					{
						cursize += n;
						iteration++;

						ret = lpc_senddata(dev, dfu_buf, n);

						if (mylog_fh != stdout)
						{
							putchar('o');
							fflush(stdout);
						}

						MY_LOG_NOF(4, MYLOG_MASK_LPCDFU, "Cycle %3lu - Transfer %5lu bytes (Total %7lu/%7lu bytes)\n",
								iteration, (unsigned long)n, cursize, fsize);
					}
				}
			} // while
		}
		fclose(fh);

		//wait for end of PROGRAM command before to exit
		if (ret == OK)
			ret = wait_dfuop_status(dev, DFU_OPSTS_IDLE, DFU_HOSTCMD_PROGRAM, 10, 90, NULL);

		if (mylog_fh != stdout)
			putchar('\n');
	}

	MY_LOG(4, MYLOG_MASK_LPCDFU, "Exit (%d)\n", ret);

	return ret;
}

int lpc_dfu_upload(struct libusb_device_handle *dev, uint32_t start_address, const char *filename, unsigned long fsize)
{
	int ret = OK;
	unsigned long tot_iter;
	FILE *fh = NULL;

	if (dev == NULL || filename == NULL || fsize == 0)
		return ERRCODE(ERR_BADPARAM);

	if ((start_address % sizeof(dfu_buf)) != 0)
	{
		MY_LOG(0, MYLOG_MASK_LPCDFU, "Start address (0x%08lx) must be 0x%08lx aligned!\n",
				(unsigned long)start_address, (unsigned long)sizeof(dfu_buf));
		return ERRCODE(ERR_BADPARAM);
	}

	if (mylog_fh != stdout)
		printf("READBACK - File Name: %s - Start: 0x%08lx - Size: %lu\n",
				filename, (unsigned long)start_address, (unsigned long)fsize);

	tot_iter = (fsize + sizeof(dfu_buf) - 1) / sizeof(dfu_buf);
	MY_LOG(4, MYLOG_MASK_LPCDFU, "READBACK - File Name: %s - Start: 0x%08lx - Size: %lu - Tot iterations: %lu\n",
			filename, (unsigned long)start_address, (unsigned long)fsize, (unsigned long)tot_iter);

	fh = fopen(filename, "wb");
	if (fh == NULL)
	{
		ret = ERRCODE(ERR_FILEOPEN);
	}
	else
	{
		if (ret == OK)
			ret = check_dfu_idlestate(dev);

		if (ret == OK)
			ret = lpc_sendcommand(dev, DFU_HOSTCMD_READBACK, start_address, fsize);

		if (ret == OK)
		{
			unsigned long cursize;
			unsigned long iteration;
			int rc, n = 0;

			cursize = 0;
			iteration = 0;

			while (ret == OK && cursize < fsize)
			{
				int skip = 0;

				ret = wait_dfuop_status(dev, DFU_OPSTS_READTRIG, DFU_HOSTCMD_READBACK, 1, 100, print_hash);

				if (ret == OK)
				{
					n = fsize - cursize;
					if (n > (int)sizeof(dfu_buf))
						n = sizeof(dfu_buf);

					MY_LOG(4, MYLOG_MASK_LPCDFU, "Upload %d bytes\n", n);

					rc = dfu_upload(dev, 0, n, 0, dfu_buf);
					if (rc < n)
					{
						if ((rc == 80 || rc == 16) && le_ptr_to_uint32(dfu_buf + 12) == 0x800)
						{
							MY_LOG(0, MYLOG_MASK_LPCDFU, "skip unexpected dfuop response packet\n");
							skip = 1;
						}
						else
						{
							fprintf(stderr, "DFU Upload data error (%d)\n", rc);
							ret = ERRCODE(ERR_DFUUPLOAD);
						}
					}
				}

				if (ret == OK && !skip)
				{
					MY_LOG(6, MYLOG_MASK_LPCDFU, "Received %d bytes\n", rc);
					mylog_hexbuffer(7, MYLOG_MASK_LPCDFU, dfu_buf, n);

					rc = fwrite(dfu_buf, 1, n, fh);
					if (rc != n || ferror(fh))
					{
						perror("Error during file write");
						ret = ERRCODE(ERR_FILEWRITE);
					}
					else
					{
						cursize += n;
						iteration++;

						if (mylog_fh != stdout)
						{
							putchar('o');
							fflush(stdout);
						}

						MY_LOG_NOF(4, MYLOG_MASK_LPCDFU, "Cycle %3lu - Transfer %5lu bytes (Total %7lu/%7lu bytes)\n",
								iteration, (unsigned long)n, cursize, fsize);
					}
				}
			} // while
		}
		if (ret == OK)	//Se a questo punto non siamo in IDLE significa un probabile mismatch delle lunghezze tra host e device
			ret = wait_dfuop_status(dev, DFU_OPSTS_IDLE, DFU_HOSTCMD_READBACK, 1, 100, print_hash);

		fclose(fh);

		if (mylog_fh != stdout)
			putchar('\n');
	}

	MY_LOG(4, MYLOG_MASK_LPCDFU, "Exit (%d)\n", ret);

	return ret;
}

//Buld erase of all flash memory
int lpc_dfu_erase_all(struct libusb_device_handle *dev)
{
	int ret = OK;

	MY_LOG(5, MYLOG_MASK_LPCDFU, "Enter\n");

	if (dev == NULL)
		return ERRCODE(ERR_BADPARAM);

	if (mylog_fh != stdout)
		printf("ERASE-ALL\n");

	if (ret == OK)
		ret = check_dfu_idlestate(dev);

	if (ret == OK)
		ret = lpc_sendcommand(dev, DFU_HOSTCMD_ERASE_ALL, 0, 0);

	if (ret == OK)
		ret = wait_dfuop_status(dev, DFU_OPSTS_IDLE, DFU_HOSTCMD_ERASE_ALL, 200, 5 * 60 * 3, print_dot);	//about 3min timeout

	if (mylog_fh != stdout)
		putchar('\n');

	MY_LOG(4, MYLOG_MASK_LPCDFU, "Exit (%d)\n", ret);

	return ret;
}

int lpc_dfu_erase(struct libusb_device_handle *dev, uint32_t start_address, uint32_t esize)
{
	int ret = OK;

	MY_LOG(5, MYLOG_MASK_LPCDFU, "Enter\n");

	if (dev == NULL || esize == 0)
		return ERRCODE(ERR_BADPARAM);

	if (mylog_fh != stdout)
		printf("ERASE REGION\n");

//	if ((start_address % flash_erase_size) != 0)
//	{
//		fprintf(stderr, "Start address (0x%08lx) must be 0x%08lx aligned!\n", (unsigned long)start_address, (unsigned long)flash_erase_size);
//		return ERRCODE(ERR_BADPARAM);
//	}

	if (ret == OK)
		ret = check_dfu_idlestate(dev);

	if (ret == OK)
		ret = lpc_sendcommand(dev, DFU_HOSTCMD_ERASE_REGION, start_address, esize);

	if (ret == OK)
		ret = wait_dfuop_status(dev, DFU_OPSTS_IDLE, DFU_HOSTCMD_ERASE_REGION, 200, 5 * 60 * 3, print_dot);	//about 3min timeout

	if (mylog_fh != stdout)
		putchar('\n');

	MY_LOG(4, MYLOG_MASK_LPCDFU, "Exit (%d)\n", ret);

	return ret;
}

int lpc_dfu_reset(struct libusb_device_handle *dev)
{
	int ret = OK;

	MY_LOG(5, MYLOG_MASK_LPCDFU, "Enter\n");

	if (dev == NULL)
		return ERRCODE(ERR_BADPARAM);

	if (mylog_fh != stdout)
		printf("RESET\n");

	ret = check_dfu_idlestate(dev);
	if (ret == OK)
		ret = lpc_sendcommand(dev, DFU_HOSTCMD_RESET, 0, 0);

	MY_LOG(5, MYLOG_MASK_LPCDFU, "Exit (%d)\n", ret);

	return ret;
}

int lpc_dfu_jumptoapp(struct libusb_device_handle *dev)
{
	int ret = OK;

	MY_LOG(5, MYLOG_MASK_LPCDFU, "Enter\n");

	if (dev == NULL)
		return ERRCODE(ERR_BADPARAM);

	if (mylog_fh != stdout)
		printf("JUMP TO APP\n");

	ret = check_dfu_idlestate(dev);
	if (ret == OK)
		ret = lpc_sendcommand(dev, DFU_HOSTCMD_EXECUTE, 0, 0);

	MY_LOG(5, MYLOG_MASK_LPCDFU, "Exit (%d)\n", ret);

	return ret;
}

//Init stuff: open log file, check idle status, set device debug
int lpc_dfu_prepare(struct libusb_device_handle *dev, int enable_debug, const char *filename)
{
	int ret;

	MY_LOG(5, MYLOG_MASK_LPCDFU, "Enter\n");

	if (dev == NULL || filename == NULL)
		return ERRCODE(ERR_BADPARAM);

	if (mylog_fh != stdout)
		printf("PREPARE\n");

	fhlog = fopen(filename, "w");

	ret = check_dfu_idlestate(dev);
	if (ret == OK)
		ret = lpc_sendcommand(dev, DFU_HOSTCMD_SETDEBUG, enable_debug ? 0 : 1, 0);

	MY_LOG(5, MYLOG_MASK_LPCDFU, "Exit (%d)\n", ret);

	return ret;
}

//Cleanup stuff
int lpc_dfu_exit(struct libusb_device_handle *dev)
{
	int ret = OK;

	(void)dev;

	MY_LOG(4, MYLOG_MASK_LPCDFU, "Enter\n");

	if (fhlog)
	{
		fclose(fhlog);
		fhlog = NULL;
	}

	MY_LOG(4, MYLOG_MASK_LPCDFU, "Exit (%d)\n", ret);

	return ret;
}

static int check_dfu_idlestate(struct libusb_device_handle *dev)
{
	int ret;
	bool do_loop;
	int timeout;

	MY_LOG(6, MYLOG_MASK_LPCDFU, "Enter\n");

	if (dev == NULL)
		return ERRCODE(ERR_BADPARAM);

	do_loop = true;
	timeout = 0;
	ret = lpc_dfu_get_status(dev, STATE_DFU_IDLE);
	while (ret == OK && do_loop && ++timeout <= 20)
	{
		uint32_t cmd, progstatus;

		ret = lpc_getopstate(dev, &cmd, &progstatus, &do_loop);
		if (ret == OK)
		{
			if (progstatus != DFU_OPSTS_IDLE)
			{
				fprintf(stderr, "DFU Not in IDLE state before command\n");
				ret = ERRCODE(ERR_DFUDEVBUSY);
			}
		}
	}
	MY_LOG(5, MYLOG_MASK_LPCDFU, "Exit (%d)\n", ret);

	return ret;
}

//Wait for the device goes to the state 'dfu_opsts' after the command 'dfu_cmd'
// 'ms_to_sleep' is the number of milliseconds to wait for every pool loop
// 'timeout' is the number of loop cycles before timeout, so the timeout period is calculated 'ms_to_sleep' x 'timeout'
// optional 'feedback' callback for status indication or progress bar.
static int wait_dfuop_status(struct libusb_device_handle *dev, DFU_OPSTS_T dfu_opsts, DFU_HOSTCMD_T dfu_cmd,
								int ms_to_sleep, int timeout, void (*feedback)(int loop))
{
	int ret = OK;
	bool wait;
	uint32_t cmd, progstatus = DFU_OPSTS_IDLE;

	MY_LOG(5, MYLOG_MASK_LPCDFU, "Enter\n");

//	ret = lpc_dfu_get_status(dev, -1);

	wait = true;
	while (ret == OK && wait && timeout > 0)
	{
		if (timeout > 0)
			timeout--;

		if (ms_to_sleep > 0)
			milli_sleep(ms_to_sleep);

		ret = lpc_getopstate(dev, &cmd, &progstatus, NULL);
		if (ret == OK && progstatus == dfu_opsts && (dfu_cmd == DFU_HOSTCMD_INVALID || dfu_cmd == cmd))
		{
			wait = false;
		}
		else
		{
			MY_LOG(4, MYLOG_MASK_LPCDFU, "Wait for Status 0x%02x [%s], current Status 0x%02x [%s] (countdown=%d)\n",
					dfu_opsts, dfuop_status_to_string(dfu_opsts),
					progstatus, dfuop_status_to_string(progstatus), timeout);
			if (feedback != NULL)
				feedback(timeout);
		}
	}
	if (ret == OK && wait)
	{	//timeout
		ret = ERRCODE(ERR_TIMEOUT);
	}
	MY_LOG(4, MYLOG_MASK_LPCDFU, "Exit, Status=0x%02x [%s], ret=%d\n",
			progstatus, dfuop_status_to_string(progstatus), ret);

	return ret;
}

//Request the device status (not the DFU status), implemented by a DFU_UPLOAD
static int lpc_getopstate(struct libusb_device_handle *dev, uint32_t *pcmd, uint32_t *pstatus, bool *phave_dbgstr)
{
	int ret = OK;
	uint32_t cmd, status, nschar, res;
	bool have_dbgstr = false;
	uint8_t buf[256];
	int msg_idx, rc;
	static char msg_buf[MSG_BUFSIZE];

	MY_LOG(5, MYLOG_MASK_LPCDFU, "Enter\n");

	if (dev == NULL)
		return ERRCODE(ERR_BADPARAM);

	msg_idx = 0;
	cmd = status = nschar = res = 0;
	rc = dfu_upload(dev, 0, sizeof(buf), 0, buf);
	if (rc < 16 || rc > 80)		//some check on packet len for valid getopstate response
	{
		fprintf(stderr, "Upload error during getopstate (%d)\n", rc);
		ret = ERRCODE(ERR_DFUUPLOAD);
	}
	else
	{
		cmd = le_ptr_to_uint32(buf + 0);
		status = le_ptr_to_uint32(buf + 4);
		nschar = le_ptr_to_uint32(buf + 8);
		res = le_ptr_to_uint32(buf + 12);

		//Check for valid fields
		if ((cmd & 0xffffff00) != 0 || (status & 0xffffff00) != 0 || (nschar & 0xffffff00) != 0 || res != 0x800)
		{
			MY_LOG(2, MYLOG_MASK_LPCDFU,
					"Invalid response: CmdResponse=0x%04x, progStatus=0x%04x, strBytes=0x%04x, reserved=0x%04x\n",
					cmd, status, nschar, res);
			mylog_hexbuffer(5, MYLOG_MASK_LPCDFU, buf, 16);
			fprintf(stderr, "Upload Op State, invalid response\n");
			ret = ERRCODE(ERR_DFUUPLOAD);
		}
		else
		{
			MY_LOG(6, MYLOG_MASK_LPCDFU, "Received %d bytes\n", rc);
			MY_LOG(6, MYLOG_MASK_LPCDFU,
					"CmdResponse=0x%04x, progStatus=0x%04x, strBytes=0x%04x, reserved=0x%04x\n",
					cmd, status, nschar, res);
			mylog_hexbuffer(7, MYLOG_MASK_LPCDFU, buf, 16);

			if (nschar > 0 && nschar <= 0x40)
			{
				memcpy(msg_buf + msg_idx, buf + 16, nschar);
				msg_idx += nschar;
			}
			have_dbgstr = (nschar >= 0x40);
		}
	}

	msg_buf[msg_idx++] = '\0';
	if (fhlog != NULL && strlen(msg_buf) > 0)
	{
		fputs(msg_buf, fhlog);
		fflush(fhlog);
	}

	if (pcmd != NULL)
		*pcmd = cmd;
	if (pstatus != NULL)
		*pstatus = status;
	if (phave_dbgstr != NULL)
		*phave_dbgstr = have_dbgstr;

	MY_LOG(5, MYLOG_MASK_LPCDFU, "Ret=%d, cmd=0x%02x, status=0x%02x, have_dbgstr=%d\n", ret, cmd, status, have_dbgstr);

	return ret;
}

static int lpc_sendcommand(struct libusb_device_handle *dev, uint32_t cmd, uint32_t addr, uint32_t siz)
{
	int ret = OK;
	int wc, transaction = 0;
	uint8_t buf[16];

	if (dev == NULL)
		return ERRCODE(ERR_BADPARAM);

	MY_LOG(4, MYLOG_MASK_LPCDFU, "Send command: 0x%02x, addr:0x%08x, size:0x%08x\n", cmd, addr, siz);

	le_uint32_to_ptr(buf + 0, cmd);
	le_uint32_to_ptr(buf + 4, addr);
	le_uint32_to_ptr(buf + 8, siz);
	le_uint32_to_ptr(buf + 12, DFUPROG_VALIDVAL);
	wc = dfu_download(dev, 0, sizeof(buf), transaction++, buf);
	if (wc < 0)
	{
		fprintf(stderr, "Error during download 0x%02x command (%d)\n", cmd, wc);
		ret = ERRCODE(ERR_DFUDOWNLOAD);
	}

	if (ret == OK)
		ret = lpc_dfu_get_status(dev, STATE_DFU_DOWNLOAD_IDLE);

	if (ret == OK)
	{
		MY_LOG(5, MYLOG_MASK_LPCDFU, "Send ZERO pk\n");
		wc = dfu_download(dev, 0, 0, transaction++, NULL);
		if (wc < 0)
		{
			fprintf(stderr, "Error during download ZLP after 0x%02x command (%d)\n", cmd, wc);
			ret = ERRCODE(ERR_DFUDOWNLOAD);
		}
	}

	if (ret == OK)
		ret = lpc_dfu_get_status(dev, STATE_DFU_IDLE);

	MY_LOG(5, MYLOG_MASK_LPCDFU, "Exit (%d)\n", ret);

	return ret;
}

static int lpc_senddata(struct libusb_device_handle *dev, uint8_t *buf, unsigned int len)
{
	int ret = OK;
	int wc, transaction = 0;

	MY_LOG(5, MYLOG_MASK_LPCDFU, "Download packet (len=%u)\n", len);
	wc = dfu_download(dev, 0, len, transaction++, buf);
	if (wc < 0)
	{
		fprintf(stderr, "Error during download data (%d)\n", wc);
		ret = ERRCODE(ERR_DFUDOWNLOAD);
	}

	if (ret == OK)
		ret = lpc_dfu_get_status(dev, STATE_DFU_DOWNLOAD_IDLE);

	if (ret == OK)
	{
		MY_LOG(5, MYLOG_MASK_LPCDFU, "Send ZERO pk\n");
		wc = dfu_download(dev, 0, 0, transaction++, NULL);
		if (wc < 0)
		{
			fprintf(stderr, "Error during download ZLP after data packet (%d)\n", wc);
			ret = ERRCODE(ERR_DFUDOWNLOAD);
		}
	}

	if (ret == OK)
		ret = lpc_dfu_get_status(dev, STATE_DFU_IDLE);

	MY_LOG(5, MYLOG_MASK_LPCDFU, "Exit (%d)\n", ret);

	return ret;
}

static int lpc_dfu_get_status(struct libusb_device_handle *dev, int expected_state)
{
	int err, ret = OK;
	struct dfu_status sta;

	if (expected_state == -1)
		expected_state = STATE_DFU_IDLE;

	MY_LOG(5, MYLOG_MASK_LPCDFU, "Enter (state=%d)\n", expected_state);

	memset(&sta, 0, sizeof(sta));
	err = dfu_get_status(dev, 0, &sta);
//	if (err == LIBUSB_ERROR_PIPE)
//	{
//		printf("Device does not implement get_status, assuming appIDLE\n");
//		sta.bStatus = DFU_STATUS_OK;
//		sta.bwPollTimeout = 0;
//		sta.bState  = STATE_DFU_IDLE;
//		sta.iString = 0;
//	}
//	else
	if (err < 0)
	{
		fprintf(stderr, "dfu_get_status() failed with error %d\n", err);
		ret = ERRCODE(ERR_DFUGETSTATUS);
	}
	else
	{
		MY_LOG(5, MYLOG_MASK_LPCDFU, "GetStatus: return bStatus=0x%02x [%s], bState=0x%02x [%s], bwPollTimeout=%u\n",
				sta.bStatus, dfu_status_to_string(sta.bStatus), sta.bState, dfu_state_to_string(sta.bState),
				sta.bwPollTimeout);

		if (sta.bState != expected_state)
		{
			fprintf(stderr, "Invalid DFU state: 0x%02x [%s] --- Expected state: 0x%02x [%s]\n",
					sta.bState, dfu_state_to_string(sta.bState), expected_state, dfu_state_to_string(expected_state));
			ret = ERRCODE(ERR_DFUBADSTATE);
		}
		else
		if (sta.bStatus != DFU_STATUS_OK)
		{
			fprintf(stderr, "Invalid DFU status: 0x%02x [%s]\n", sta.bStatus, dfu_status_to_string(sta.bStatus));
			ret = ERRCODE(ERR_DFUBADSTATUS);
		}
	}
	MY_LOG(5, MYLOG_MASK_LPCDFU, "Exit (%d)\n", ret);

	return ret;
}

static void le_uint32_to_ptr(uint8_t *p, uint32_t val)
{
	p[0] = (uint8_t)(val >> 0);
	p[1] = (uint8_t)(val >> 8);
	p[2] = (uint8_t)(val >> 16);
	p[3] = (uint8_t)(val >> 24);
}

static uint32_t le_ptr_to_uint32(const uint8_t *p)
{
	uint32_t val;

	val =	((uint32_t)p[0] << 0) |
			((uint32_t)p[1] << 8) |
			((uint32_t)p[2] << 16) |
			((uint32_t)p[3] << 24);

	return val;
}

/**
static void be_uint32_to_ptr(uint8_t *p, uint32_t val)
{
	p[3] = (uint8_t)(val >> 0);
	p[2] = (uint8_t)(val >> 8);
	p[1] = (uint8_t)(val >> 16);
	p[0] = (uint8_t)(val >> 24);
}

static uint32_t be_ptr_to_uint32(const uint8_t *p)
{
	uint32_t val;

	val =	((uint32_t)p[3] << 0) |
			((uint32_t)p[2] << 8) |
			((uint32_t)p[1] << 16) |
			((uint32_t)p[0] << 24);

	return val;
}
**/

static const char *dfuop_status_to_string(DFU_OPSTS_T sts)
{
	switch (sts)
	{
	case DFU_OPSTS_IDLE:
		return "Idle, can accept new host command";
	case DFU_OPSTS_ERRER:
		return "Erase error";
	case DFU_OPSTS_PROGER:
		return "Program error";
	case DFU_OPSTS_READER:
		return "Readback error";
	case DFU_OPSTS_ERRUN:
		return "Unknown error";
	case DFU_OPSTS_READBUSY:
		return "Device is busy reading a block of data";
	case DFU_OPSTS_READTRIG:
		return "Device data is ready to read";
	case DFU_OPSTS_READREADY:
		return "Block of data is ready";
	case DFU_OPSTS_ERASE_ALL_ST:
		return "Device is about to start full erase";
	case DFU_OPSTS_ERASE_ST:
		return "Device is about to start region erase";
	case DFU_OPSTS_ERASE:
		return "Device is currently erasing";
	case DFU_OPSTS_PROG:
		return "Device is currently programming a range";
	case DFU_OPSTS_PROG_RSVD:
		return "Reserved state, not used";
	case DFU_OPSTS_PROG_STREAM:
		return "Device is in buffer streaming mode";
	case DFU_OPSTS_RESET:
		return "Will shutdown and reset";
	case DFU_OPSTS_EXEC:
		return "Will shutdown USB and start execution";
	case DFU_OPSTS_LOOP:
		return "Loop on error after DFU status check";
	default:
		return "???";
	}
}

static void print_dot(int loop)
{
	(void)loop;

	if (mylog_fh != stdout)
	{
		putchar('.');
		fflush(stdout);
	}
}

static void print_hash(int loop)
{
	(void)loop;

	if (mylog_fh != stdout)
	{
		putchar('#');
		fflush(stdout);
	}
}
