/* -*- c -*-
 * init.c --
 *
 *	Implements the C level procedures handling the initialization of
 *	this package
 *
 *
 * Copyright (c) 1996-1999 Andreas Kupries (a.kupries@westend.com)
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL I LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
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
 * CVS: $Id: init.c,v 1.4 1999/06/12 21:54:49 aku Exp $
 */

#include "memchanInt.h"

/*
 *------------------------------------------------------*
 *
 *	Memchan_Init --
 *
 *	------------------------------------------------*
 *	Standard procedure required by 'load'. 
 *	Initializes this extension.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As of 'Tcl_CreateCommand'.
 *
 *	Result:
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

EXPORT (int,Memchan_Init) (interp)
Tcl_Interp* interp;
{
#if GT81
  if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
    return TCL_ERROR;
  }
#endif

  Tcl_CreateObjCommand (interp, "memchan",
			&MemchanCmd,
			(ClientData) NULL,
			(Tcl_CmdDeleteProc*) NULL);

  Tcl_CreateObjCommand (interp, "fifo",
			&MemchanFifoCmd,
			(ClientData) NULL,
			(Tcl_CmdDeleteProc*) NULL);

  /* register memory channels as available package */
  Tcl_PkgProvide (interp, "Memchan", MEMCHAN_VERSION);

  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	Memchan_SafeInit --
 *
 *	------------------------------------------------*
 *	Standard procedure required by 'load'. 
 *	Initializes this extension for a safe interpreter.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As of 'Memchan_Init'
 *
 *	Result:
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

EXPORT (int,Memchan_SafeInit) (interp)
Tcl_Interp* interp;
{
  return Memchan_Init (interp);
}

