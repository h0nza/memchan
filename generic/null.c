/*
 * null.c --
 *
 *	Implementation of a null device.
 *
 * Copyright (C) 2000 Andreas Kupries (a.kupries@westend.com)
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
 * CVS: $Id: null.c,v 1.4 2004/05/20 19:08:30 andreas_kupries Exp $
 */


#include "memchanInt.h"

/*
 * Forward declarations of internal procedures.
 */

static int	Close _ANSI_ARGS_((ClientData instanceData,
		   Tcl_Interp *interp));

static int	Input _ANSI_ARGS_((ClientData instanceData,
		    char *buf, int toRead, int *errorCodePtr));

static int	Output _ANSI_ARGS_((ClientData instanceData,
	            CONST84 char *buf, int toWrite, int *errorCodePtr));

static void	WatchChannel _ANSI_ARGS_((ClientData instanceData, int mask));
static void	ChannelReady _ANSI_ARGS_((ClientData instanceData));
static int      GetFile      _ANSI_ARGS_((ClientData instanceData,
					  int direction,
					  ClientData* handlePtr));

static int	BlockMode _ANSI_ARGS_((ClientData instanceData,
				       int mode));

/*
 * This structure describes the channel type structure for null channels:
 * Null channels are not seekable. They have no options.
 */

static Tcl_ChannelType channelType = {
  "null",		/* Type name.                                    */
  BlockMode,		/* Set blocking/nonblocking behaviour. */
  Close,		/* Close channel, clean instance data            */
  Input,		/* Handle read request                           */
  Output,		/* Handle write request                          */
  NULL,			/* Move location of access point.      NULL'able */
  NULL,			/* Set options.                        NULL'able */
  NULL,			/* Get options.                        NULL'able */
  WatchChannel,		/* Initialize notifier                           */
#if GT81
  GetFile,              /* Get OS handle from the channel.               */
  NULL                  /* Close2Proc, not available, no partial close
			 * possible */
#else
  GetFile               /* Get OS handle from the channel.               */
#endif
};


/*
 * This structure describes the per-instance state of a in-memory null channel.
 */

typedef struct ChannelInstance {
  Tcl_Channel    chan;   /* Backreference to generic channel information */
  Tcl_TimerToken timer;  /* Timer used to link the channel into the
			  * notifier. */
} ChannelInstance;

/*
 *----------------------------------------------------------------------
 *
 * BlockMode --
 *
 *	Helper procedure to set blocking and nonblocking modes on a
 *	memory channel. Invoked by generic IO level code.
 *
 * Results:
 *	0 if successful, errno when failed.
 *
 * Side effects:
 *	Sets the device into blocking or non-blocking mode.
 *
 *----------------------------------------------------------------------
 */

static int
BlockMode (instanceData, mode)
     ClientData instanceData;
     int mode;
{
    return 0;
}

/*
 *------------------------------------------------------*
 *
 *	Close --
 *
 *	------------------------------------------------*
 *	This procedure is called from the generic IO
 *	level to perform channel-type-specific cleanup
 *	when an in-memory null channel is closed.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Closes the device of the channel.
 *
 *	Result:
 *		0 if successful, errno if failed.
 *
 *------------------------------------------------------*
 */
/* ARGSUSED */
static int
Close (instanceData, interp)
ClientData  instanceData;    /* The instance information of the channel to
			      * close */
Tcl_Interp* interp;          /* unused */
{

  ChannelInstance* chan;

  chan = (ChannelInstance*) instanceData;

  if (chan->timer != (Tcl_TimerToken) NULL) {
    Tcl_DeleteTimerHandler (chan->timer);
  }

  Tcl_Free ((char*) chan);

  return 0;
}

/*
 *------------------------------------------------------*
 *
 *	Input --
 *
 *	------------------------------------------------*
 *	This procedure is invoked from the generic IO
 *	level to read input from an in-memory null channel.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Reads input from the input device of the
 *		channel.
 *
 *	Result:
 *		The number of bytes read is returned or
 *		-1 on error. An output argument contains
 *		a POSIX error code if an error occurs, or
 *		zero.
 *
 *------------------------------------------------------*
 */

static int
Input (instanceData, buf, toRead, errorCodePtr)
ClientData instanceData;	/* The channel to read from */
char*      buf;			/* Buffer to fill */
int        toRead;		/* Requested number of bytes */
int*       errorCodePtr;	/* Location of error flag */
{
  /* Nothing can be read from this channel.
   */

  return 0;
}

/*
 *------------------------------------------------------*
 *
 *	Output --
 *
 *	------------------------------------------------*
 *	This procedure is invoked from the generic IO
 *	level to write output to a file channel.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Writes output on the output device of
 *		the channel.
 *
 *	Result:
 *		The number of bytes written is returned
 *		or -1 on error. An output argument
 *		contains a POSIX error code if an error
 *		occurred, or zero.
 *
 *------------------------------------------------------*
 */

static int
Output (instanceData, buf, toWrite, errorCodePtr)
ClientData instanceData;	/* The channel to write to */
CONST84 char* buf;		/* Data to be stored. */
int        toWrite;		/* Number of bytes to write. */
int*       errorCodePtr;	/* Location of error flag. */
{
  /* Everything which is written to this channel disappears.
   */

  return toWrite;
}

/*
 *------------------------------------------------------*
 *
 *	WatchChannel --
 *
 *	------------------------------------------------*
 *	Initialize the notifier to watch Tcl_Files from
 *	this channel.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Sets up the notifier so that a future
 *		event on the channel will be seen by Tcl.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */
	/* ARGSUSED */
static void
WatchChannel (instanceData, mask)
ClientData instanceData;	/* Channel to watch */
int        mask;		/* Events of interest */
{
  /*
   * null channels are not based on files.
   * They are always writable, and always readable.
   * We could call Tcl_NotifyChannel immediately, but this
   * would starve other sources, so a timer is set up instead.
   */

  ChannelInstance* chan = (ChannelInstance*) instanceData;

  if (mask) {
    chan->timer = Tcl_CreateTimerHandler (DELAY, ChannelReady, instanceData);
  } else {
    Tcl_DeleteTimerHandler (chan->timer);
    chan->timer = (Tcl_TimerToken) NULL;
  }
}

/*
 *------------------------------------------------------*
 *
 *	ChannelReady --
 *
 *	------------------------------------------------*
 *	Called by the notifier (-> timer) to check whether
 *	the channel is readable or writable.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As of 'Tcl_NotifyChannel'.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
ChannelReady (instanceData)
ClientData instanceData; /* Channel to query */
{
  /*
   * In-memory null channels are always writable (fileevent
   * writable) and they are readable if they are not empty.
   */

  ChannelInstance* chan = (ChannelInstance*) instanceData;
  int              mask = TCL_READABLE | TCL_WRITABLE;

  /*
   * Timer fired, our token is useless now.
   */

  chan->timer = (Tcl_TimerToken) NULL;

  /* Tell Tcl about the possible events.
   * This will regenerate the timer too, via 'WatchChannel'.
   */

  Tcl_NotifyChannel (chan->chan, mask);
}

/*
 *------------------------------------------------------*
 *
 *	GetFile --
 *
 *	------------------------------------------------*
 *	Called from Tcl_GetChannelHandle to retrieve
 *	OS handles from inside a in-memory null channel.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		The appropriate OS handle or NULL if not
 *		present. 
 *
 *------------------------------------------------------*
 */
static int
GetFile (instanceData, direction, handlePtr)
ClientData  instanceData;	/* Channel to query */
int         direction;		/* Direction of interest */
ClientData* handlePtr;          /* Space to the handle into */
{
  /*
   * In-memory null channels are not based on files.
   */

  /* *handlePtr = (ClientData) NULL; */
  return TCL_ERROR;
}

/*
 *------------------------------------------------------*
 *
 *	MemchanNullCmd --
 *
 *	------------------------------------------------*
 *	This procedure realizes the 'null' command.
 *	See the manpages for details on what it does.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		See the user documentation.
 *
 *	Result:
 *		A standard Tcl result.
 *
 *------------------------------------------------------*
 */
	/* ARGSUSED */
int
MemchanNullCmd (notUsed, interp, objc, objv)
ClientData    notUsed;		/* Not used. */
Tcl_Interp*   interp;		/* Current interpreter. */
int           objc;		/* Number of arguments. */
Tcl_Obj*CONST objv[];		/* Argument objects. */
{
  Tcl_Obj*         channelHandle;
  Tcl_Channel      chan;
  ChannelInstance* instance;

  if (objc != 1) {
    Tcl_AppendResult (interp,
		      "wrong # args: should be \"null\"",
		      (char*) NULL);
    return TCL_ERROR;
  }

  instance      = (ChannelInstance*) Tcl_Alloc (sizeof (ChannelInstance));
  channelHandle = MemchanGenHandle ("null");

  chan = Tcl_CreateChannel (&channelType,
			    Tcl_GetStringFromObj (channelHandle, NULL),
			    (ClientData) instance,
			    TCL_READABLE | TCL_WRITABLE);

  instance->chan      = chan;
  instance->timer     = (Tcl_TimerToken) NULL;

  Tcl_RegisterChannel  (interp, chan);
  Tcl_SetChannelOption (interp, chan, "-buffering", "none");
  Tcl_SetChannelOption (interp, chan, "-blocking",  "0");

  Tcl_SetObjResult     (interp, channelHandle);
  return TCL_OK;
}
