/************************************************
 * $Id: mydefs.h,v 1.3 2013/09/10 15:53:18 claudio Exp $
 ***********************************************/

#ifndef _MYDEFS_H_INC
#define _MYDEFS_H_INC

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef __GNUC__
#error "Supportato solo GNU GCC"
#endif

#ifndef PACKED
# define PACKED	__attribute__ ((packed))
#endif

#define HIDDEN	static
#define INLINE	inline

// Force a compilation error if condition is true
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2 * !!(condition)]))

/* Force a compilation error if condition is true, but also produce a
   result (of value 0 and type size_t), so the expression can be used
   e.g. in a structure initializer (or where-ever else comma expressions
   aren't permitted). */
#define BUILD_BUG_ON_ZERO(e) (sizeof(char[1 - 2 * !!(e)]) - 1)

#ifndef __cplusplus
	#ifdef __GNUC__
		// &a[0] degrades to a pointer: a different type from an array
		#define __must_be_array(a) \
			BUILD_BUG_ON_ZERO(__builtin_types_compatible_p(typeof(a), typeof(&a[0])))
	#else
		#define __must_be_array(a) 0
	#endif

	#define ArraySize(arr)	(sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))
#else
	#if 0	// only valid if compiled with -std=c++0x
		template <typename T, size_t N>
		constexpr size_t _array_size(T (&)[N]) { return N; }
		#define ArraySize(arr)	_array_size(arr)
	#else
		//Tricky but valid even in old C++
		// public interface
		#define ArraySize(x)	(												\
			0 * sizeof( reinterpret_cast<const ::Bad_arg_to_COUNTOF*>(x) ) +	\
			0 * sizeof( ::Bad_arg_to_COUNTOF::check_type((x), &(x)) ) +			\
			sizeof(x) / sizeof((x)[0])											\
		)

		// implementation details
		struct Bad_arg_to_COUNTOF
		{
			class Is_pointer;	// intentionally incomplete type
			class Is_array {};
			template <typename T>
			static Is_pointer check_type(const T*, const T* const*);
			static Is_array check_type(const void*, const void*);
		};
	#endif
#endif

#define KiB(x)	((x) * 1024L)
#define MiB(x)	(KiB(x) * 1024L)

/**
 * ContainerOf - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define ContainerOf(ptr, type, member) ({ \
	const typeof( ((type *)0)->member ) *__mptr = (ptr); \
	(type *)( (char *)__mptr - offsetof(type,member) );})

#define FieldSizeOf(t, f) (sizeof(((t *)0)->f))

#endif
