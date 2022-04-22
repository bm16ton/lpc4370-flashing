#ifndef _LPC_DFU_API_H_INC
#define _LPC_DFU_API_H_INC

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Region address and size for a programming algorithm
 */
struct dfuprog_regzone {
	uint32_t region_addr;		/* Offset address */
	uint32_t region_size;		/* Size in bytes */
};
typedef struct dfuprog_regzone DFUPROG_REGZONE_T;

/**
 * These are possible commands from the host machine
 */
typedef enum {
	DFU_HOSTCMD_SETDEBUG = 0,		/* Enables/disables debug output */
	DFU_HOSTCMD_ERASE_ALL = 1,		/* Erase the entire device */
	DFU_HOSTCMD_ERASE_REGION = 2,	/* Erase a region defined with addr/size */
	DFU_HOSTCMD_PROGRAM = 3,		/* Program a region defined with addr/size */
	DFU_HOSTCMD_READBACK = 4,		/* Read a region defined with addr/size */
	DFU_HOSTCMD_RESET = 5,			/* Reset the device/board */
	DFU_HOSTCMD_EXECUTE = 6,		/* Execute code at address addr */
	DFU_HOSTCMD_INVALID
} DFU_HOSTCMD_T;

/**
 * Host DFU download packet header. This is appended to a data packet when
 * programming a region.
 */
struct dfu_fromhost_pkhdr {
	uint32_t hostCmd;			/* Host command */
	uint32_t addr;				/* Start of program/erase/read region, or execute address */
	uint32_t size;				/* Size of program/erase/read region */
	uint32_t magic;				/* Should be DFUPROG_VALIDVAL */
};
typedef struct dfu_fromhost_pkhdr DFU_FROMHOST_PACKETHDR_T;

/**
 * Magic value used to indicate DFU programming algorithm and DFU Utility
 * support. This is used to lock algorithm support to DFU Utility tool
 * version to prevent issues with non-compatible versions. The upper
 * 16 bits contain a magic number and the lower 16 bits contain the
 * version number in x.y format (1.10 = 0x010A).
 */
#define DFUPROG_VALIDVAL (0x18430000 | (0x010A))

/**
 * DFU operational status returned from programming algorithm, used
 * by host to monitor status of board
 */
typedef enum {
	DFU_OPSTS_IDLE = 0,			/* Idle, can accept new host command */
	DFU_OPSTS_ERRER = 1,		/* Erase error */
	DFU_OPSTS_PROGER = 2,		/* Program error */
	DFU_OPSTS_READER = 3,		/* Readback error */
	DFU_OPSTS_ERRUN = 4,		/* Unknown error */
	DFU_OPSTS_READBUSY = 5,		/* Device is busy reading a block of data */
	DFU_OPSTS_READTRIG = 6,		/* Device data is ready to read */
	DFU_OPSTS_READREADY = 7,	/* Block of data is ready */
	DFU_OPSTS_ERASE_ALL_ST = 8,	/* Device is about to start full erase */
	DFU_OPSTS_ERASE_ST = 9,		/* Device is about to start region erase */
	DFU_OPSTS_ERASE = 10,		/* Device is currently erasing */
	DFU_OPSTS_PROG = 11,		/* Device is currently programming a range */
	DFU_OPSTS_PROG_RSVD = 12,	/* Reserved state, not used */
	DFU_OPSTS_PROG_STREAM = 13,	/* Device is in buffer streaming mode */
	DFU_OPSTS_RESET = 14,		/* Will shutdown and reset */
	DFU_OPSTS_EXEC = 15,		/* Will shutdown USB and start execution */
	DFU_OPSTS_LOOP = 16			/* Loop on error after DFU status check */
} DFU_OPSTS_T;

/**
 * When sending data to the host machine, a packet header is appended to
 * the data payload to indicate the state of the target and any debug or
 * error messages.
 */
struct dfu_tohost_pkhdr {
	uint32_t cmdResponse;		/* Command responding from host */
	uint32_t progStatus;		/* Current status of system */
	uint32_t strBytes;			/* Number of bytes in string field */
	uint32_t reserved;
};
typedef struct dfu_tohost_pkhdr DFU_TOHOST_PACKETHDR_T;

#ifdef __cplusplus
}
#endif

#endif
