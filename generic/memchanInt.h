#ifndef MEMCHAN_H
/*
 * memchanInt.h --
 *
 *	Internal definitions.
 *
 * Copyright (C) 1996-1999 Andreas Kupries (a.kupries@westend.com)
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL I BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS
 * SOFTWARE AND ITS DOCUMENTATION, EVEN IF I HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * I SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND
 * I HAVE NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES,
 * ENHANCEMENTS, OR MODIFICATIONS.
 *
 * CVS: $Id: memchanInt.h,v 1.3 1999/05/25 22:27:58 aku Exp $
 */


#include <tcl.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Definitions to enable the generation of a DLL under Windows.
 * Taken from 'ftp://ftp.sunlabs.com/pub/tcl/example.zip(example.c)'
 * ** This site is no longer valid **
 * See http://www.scriptics.com/
 */

#if defined(__WIN32__)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   undef WIN32_LEAN_AND_MEAN

/*
 * VC++ has an alternate entry point called DllMain, so we need to rename
 * our entry point.
 */

#ifdef TCL_STORAGE_CLASS
# undef TCL_STORAGE_CLASS
#endif
#ifdef BUILD_memchan
# define TCL_STORAGE_CLASS DLLEXPORT
#else
# define TCL_STORAGE_CLASS DLLIMPORT
#endif

#ifndef STATIC_BUILD
#   if defined(_MSC_VER)
#       undef EXPORT
#	define EXPORT(a,b) TCL_STORAGE_CLASS a b
#	define DllEntryPoint DllMain
#   else
#	if defined(__BORLANDC__)
#	    define EXPORT(a,b) a _export b
#	else
#	    define EXPORT(a,b) a b
#	endif
#   endif
#else
#   define EXPORT(a,b) a b
#endif
#else
#   define EXPORT(a,b) a b
#endif


/*
 * Number of bytes used to extend a storage area found to small.
 */

#define INCREMENT (512)

/*
 * Number of milliseconds to wait between polls of channel state,
 * e.g. generation of readable/writable events.
 *
 * Relevant for only Tcl 8.0 and beyond.
 */

#define DELAY (5)

/* Detect Tcl 8.1 and beyond => Stubs, panic <-> Tcl_Panic
 */

#define GT81 ((TCL_MAJOR_VERSION > 8) || \
	      ((TCL_MAJOR_VERSION == 8) && \
	       (TCL_MINOR_VERSION >= 1)))

#if ! (GT81)
/* Enable use of procedure internal to tcl. Necessary only
 * for versions of tcl below 8.1.
 */

EXTERN void
panic _ANSI_ARGS_ ((char* format, ...));

#undef  Tcl_Panic
#define Tcl_Panic panic
#endif


/* Internal command visible to other parts of the package.
 */

#if TCL_MAJOR_VERSION < 8
extern int
MemchanCmd _ANSI_ARGS_ ((ClientData notUsed,
			 Tcl_Interp* interp,
			 int argc, char** argv));

extern int
MemchanFifoCmd _ANSI_ARGS_ ((ClientData notUsed,
			     Tcl_Interp* interp,
			     int argc, char** argv));
#define ARGC argc
#else
extern int
MemchanCmd _ANSI_ARGS_ ((ClientData notUsed,
			 Tcl_Interp* interp,
			 int objc, Tcl_Obj*CONST objv[]));

extern int
MemchanFifoCmd _ANSI_ARGS_ ((ClientData notUsed,
			     Tcl_Interp* interp,
			     int objc, Tcl_Obj*CONST objv[]));
#define ARGC objc
#endif


/* Generator procedure for handles. Handles mutex issues for a thread
 * enabled version of tcl.
 */

#if TCL_MAJOR_VERSION < 8
CONST char* MemchanGenHandle _ANSI_ARGS_ ((CONST char* prefix));
#else
Tcl_Obj*    MemchanGenHandle _ANSI_ARGS_ ((CONST char* prefix));
#endif

#ifdef __cplusplus
}
#endif /* C++ */
#endif /* MEMCHAN_H */
