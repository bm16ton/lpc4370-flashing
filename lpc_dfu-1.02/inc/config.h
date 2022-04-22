#ifndef _CONFIG_H_INC
#define _CONFIG_H_INC

#ifdef __linux__
#define HAVE_FTRUNCATE
#define HAVE_NANOSLEEP
#define HAVE_ERR
#endif

#ifdef WIN32
#define HAVE_WINDOWS_H
#endif

#endif
