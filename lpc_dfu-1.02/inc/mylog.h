/*
 * $Id: mylog.h,v 1.2 2013/11/18 15:13:04 claudio Exp $
 */

#ifndef MYLOG_H_INC
#define MYLOG_H_INC

#define MYLOG_MASK_ENABLEALL		0xffffffff
#define MYLOG_MASK_DISABLEALL		0x00000000

#define MYLOG_MASK_MAIN			(1 << 0)
#define MYLOG_MASK_LPCDFU		(1 << 1)

#if ENABLE_MYLOG == 1

# include <stdio.h>
# include <stdint.h>

	extern FILE *mylog_fh;
	extern int mylog_level;
	extern unsigned int mylog_mask;

# define MYLOG_FILE(fh)			(mylog_fh = (fh))
# define MYLOG_SETMASK(mask)		(mylog_mask = (mask))
# define MYLOG_SETLEVEL(level)		(mylog_level = (level))

	static inline void mylog_hexbuffer(int level, uint32_t mask, const void *buffer, uint32_t size)
	{
		if (level == 0 || (level <= mylog_level &&
			(mask & mylog_mask) != 0))
		{
			uint32_t k;
			const uint8_t *p = buffer;

			fprintf(mylog_fh, "Buffer start @ %p\n"
					"location: 00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f",
					buffer);

			for (k = 0; k < size; k++)
			{
				if ((k % 16) == 0)
					fprintf(mylog_fh, "\n%08lx: ", (unsigned long)k);

				fprintf(mylog_fh, "%02x ", *p++);
			}

			fprintf(mylog_fh, "\n");
		}
	}

//I msg a livello 0 (fatal errors) compaiono SEMPRE anche se il modulo e` disabilitato (per es. msg di errore)
//I msg a livello > 0 compaiono solo se il modulo e` abilitato e il livello e` sufficiente
# define MY_LOG(level, mask, fmt, args...) \
	do { \
		if ((level) == 0 || ((level) <= mylog_level && ((mask) & mylog_mask) != 0)) \
		{ \
			fprintf(mylog_fh, "%s: " fmt, __FUNCTION__, ##args); \
		} \
	} while(0)
# define MY_LOG_NOF(level, mask, fmt, args...) \
	do { \
		if ((level) == 0 || ((level) <= mylog_level && ((mask) & mylog_mask) != 0)) \
		{ \
			fprintf(mylog_fh, fmt, ##args); \
		} \
	} while(0)
#else
# define MYLOG_FILE(fh)
# define MYLOG_SETMASK(mask)
# define MYLOG_SETLEVEL(level)
# define MY_LOG(level, mask, fmt, args...) do { } while(0)
# define MY_LOG_NOF(level, mask, fmt, args...) do { } while(0)

	static inline void mylog_hexbuffer(int level, uint32_t mask, const void *buffer, uint32_t size)
	{
		(void)level;
		(void)mask;
		(void)buffer;
		(void)size;
	}

#endif

#endif /* MYLOG_H_INC */
