/*
 * counter.c --
 *
 *	Implementation of a mutex protected counter.
 *	Used to generate channel handles.
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
 * CVS: $Id: counter.c,v 1.1 1999/05/25 18:10:55 aku Exp $
 */


#include <tcl.h>
#include <errno.h>

#include "memchanInt.h"

#if TCL_MAJOR_VERSION < 8
CONST char*
#else
Tcl_Obj*
#endif
MemchanGenHandle (prefix)
CONST char* prefix;
{
  /* 3 alternatives for implementation:
   * a) Tcl before 8.x
   * b) 8.0.x (objects, non-threaded)
   * c) 8.1.x (objects, possibly threaded)
   */

  /*
   * count number of generated (memory) channels,
   * used for id generation. Ids are never reclaimed
   * and there is no dealing with wrap around. On the
   * other hand, "unsigned long" should be big enough
   * except for absolute longrunners (generate a 100 ids
   * per second => overflow will occur in 1 1/3 years).
   */

#if GT81
  TCL_DECLARE_MUTEX (memchanCounterMutex)
  static unsigned long memCounter = 0;

  char     channelName [50];
  Tcl_Obj* res = Tcl_NewStringObj ((char*) prefix, -1);

  Tcl_MutexLock (memchanCounterMutex);
  {
    sprintf (channelName, "%lu", memCounter);
    memCounter ++;
  }
  Tcl_MutexUnlock (memchanCounterMutex);

  Tcl_AppendStringsToObj (res, channelName, (char*) NULL);

  return res;

#elif  TCL_MAJOR_VERSION == 8
  static unsigned long memCounter = 0;

  char     channelName [50];
  Tcl_Obj* res = Tcl_NewStringObj ((char*) prefix, -1);

  sprintf (channelName, "%lu", memCounter);
  memCounter ++;

  Tcl_AppendStringsToObj (res, channelName, (char*) NULL);

  return res;
#else
  static unsigned long memCounter = 0;
  static char          channelName [200];

  sprintf (channelName, "%s%lu", prefix, memCounter);
  memCounter ++;

  return channelName;
#endif
}
