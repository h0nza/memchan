/*
 * fifo.c --
 *
 *	Implementation of a memory channel having fifo behaviour.
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
 * CVS: $Id: memchan.c,v 1.6 1999/05/25 18:10:56 aku Exp $
 */


#include <tcl.h>
#include <errno.h>

#include "memchanInt.h"


/*
 * Forward declarations of internal procedures.
 */

static int	Close _ANSI_ARGS_((ClientData instanceData,
		   Tcl_Interp *interp));

static int	Input _ANSI_ARGS_((ClientData instanceData,
		    char *buf, int toRead, int *errorCodePtr));

static int	Output _ANSI_ARGS_((ClientData instanceData,
	            char *buf, int toWrite, int *errorCodePtr));

static void	WatchChannel _ANSI_ARGS_((ClientData instanceData, int mask));


#if (TCL_MAJOR_VERSION < 8)
static int	GetOption _ANSI_ARGS_((ClientData instanceData,
				       char *optionName,
				       Tcl_DString *dsPtr));

static int	ChannelReady _ANSI_ARGS_((ClientData instanceData, int mask));
static Tcl_File GetFile      _ANSI_ARGS_((ClientData instanceData, int mask));

#else

static int	GetOption _ANSI_ARGS_((ClientData instanceData,
				       Tcl_Interp* interp, char *optionName,
				       Tcl_DString *dsPtr));

static void	ChannelReady _ANSI_ARGS_((ClientData instanceData));
static int      GetFile      _ANSI_ARGS_((ClientData instanceData,
					  int direction,
					  ClientData* handlePtr));
#endif

/*
 * This structure describes the channel type structure for in-memory channels:
 * Fifo are not seekable. They have no writable options, but a readable.
 */

static Tcl_ChannelType channelType = {
  "memory/fifo",	/* Type name.                                    */
  NULL,			/* Set blocking/nonblocking behaviour. NULL'able */
  Close,		/* Close channel, clean instance data            */
  Input,		/* Handle read request                           */
  Output,		/* Handle write request                          */
  NULL,			/* Move location of access point.      NULL'able */
  NULL,			/* Set options.                        NULL'able */
  GetOption,		/* Get options.                        NULL'able */
  WatchChannel,		/* Initialize notifier                           */
#if (TCL_MAJOR_VERSION < 8)
  ChannelReady,		/* Are there events?                             */
#endif
#if GT81
  GetFile,              /* Get OS handle from the channel.               */
  NULL                  /* Close2Proc, not available, no partial close
			 * possible */
#else
  GetFile               /* Get OS handle from the channel.               */
#endif
};


/*
 * struct ChannelBuffer:
 *
 * Buffers data being sent to or from a channel.
 *
 * Copied from tcl/generic/tclIO.c (Why reinvent the wheel ?). Used here
 * to store the information placed into the channel.
 */

typedef struct ChannelBuffer {
    int nextAdded;		/* The next position into which a character
                                 * will be put in the buffer. */
    int nextRemoved;		/* Position of next byte to be removed
                                 * from the buffer. */
    int bufLength;		/* How big is the buffer? */

    struct ChannelBuffer *nextPtr;
    				/* Next buffer in chain. */
    char buf[4];		/* Placeholder for real buffer. The real
                                 * buffer occuppies this space + bufSize-4
                                 * bytes. This must be the last field in
                                 * the structure. */
} ChannelBuffer;

#define CHANNELBUFFER_HEADER_SIZE	(sizeof(ChannelBuffer) - 4)

/*
 * This structure describes the per-instance state of a in-memory fifo channel.
 */

typedef struct ChannelInstance {
  Tcl_Channel    chan;   /* Backreference to generic channel information */
  long int       length; /* Total number of bytes in the channel */
  ChannelBuffer* first;  /* Reference to the first buffer used to hold the
			  * channel data. 'nextAdded' is *not* relevant.
			  * 'nextRemoved' is used as defined. */
  ChannelBuffer* last;   /* Last buffer holding the channel data. 'nextAdded'
			  * is used as defined, 'nextRemoved' is not relevant.
			  */

#if (TCL_MAJOR_VERSION >= 8)
  Tcl_TimerToken timer;  /* Timer used to link the channel into the
			  * notifier. */
#if 0
#if (GT81) && defined (TCL_THREADS)
  Tcl_Mutex lock;        /* Semaphor to handle thread-spanning access to this
			  * fifo. */
#endif
#endif /* 0 */

#endif
} ChannelInstance;

/* Macro to check a fifo channel for emptiness.
 */

#define FIFO_EMPTY(c) ((c->first == (ChannelBuffer*) NULL) || ((c->first == c->last) && (c->first->nextRemoved > c->first->nextAdded))


/*
 *------------------------------------------------------*
 *
 *	Close --
 *
 *	------------------------------------------------*
 *	This procedure is called from the generic IO
 *	level to perform channel-type-specific cleanup
 *	when an in-memory fifo channel is closed.
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

  /* Iterate over the chain of buffers and release the
   * allocated memory. We can be sure that this is done only
   * after the last user closed the channel, i.e. there will
   * be no thread-spanning access !
   */

  while (chan->first != (ChannelBuffer*) NULL) {
    chan->last = chan->first->nextPtr;

    Tcl_Free ((char*) chan->first);
    chan->first = chan->last;
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
 *	level to read input from an in-memory fifo channel.
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
  ChannelBuffer*   cbuf;
  ChannelInstance* chan;
  long int         required;
  long int         cbufSize;

  if (toRead == 0) {
    return 0;
  }

  chan = (ChannelInstance*) instanceData;

  if (toRead > chan->length) {
    /*
     * Reading behind the last byte is not possible,
     * truncate the request.
     */
    toRead = chan->length;
  }

  while (required > 0) {
    /* Iterate over the chain until the request is satisfied.
     * Releases all buffers which were completely consumed.
     */

    cbuf     = chan->first;
    cbufSize = cbuf->bufLength - cbuf->nextRemoved;

    if (required <= cbufSize) {
      /* The current buffer satisfies the whole (remaining) request.
       * Copy as needed.
       */

      memcpy ((VOID*) buf, (VOID*) ((char*) cbuf->buf + cbuf->nextRemoved),
	      required);
      cbuf->nextRemoved += required;
      required = 0;

    } else {
      /* The current buffer is not enough. Copy everything in it, then
       * prepare ourselves for another round of the loop
       */

      memcpy ((VOID*) buf, (VOID*) ((char*) cbuf->buf + cbuf->nextRemoved),
	      cbufSize);

      cbuf->nextRemoved += cbufSize;
      required          -= cbufSize;
      buf               += cbufSize;
    }

    /* Release an empty buffer at the head of the chain.
     */

    if (cbuf->nextRemoved == cbuf->bufLength) {
      chan->first = cbuf->nextPtr;

      if (chan->first == (ChannelBuffer*) NULL) {
	chan->last = chan->first;
      }

      Tcl_Free ((char*) cbuf);
    }

    /* and around */
  }

  chan->length -= toRead;

  *errorCodePtr = 0;
  return toRead;
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
char*      buf;			/* Data to be stored. */
int        toWrite;		/* Number of bytes to write. */
int*       errorCodePtr;	/* Location of error flag. */
{
  ChannelInstance* chan;
  ChannelBuffer*   cbuf;

  if (toWrite == 0) {
    return 0;
  }

  chan = (ChannelInstance*) instanceData;

  /* Simple strategy for now: Every write to the channel is placed
   * into its own buffer, which is then appended to the chain.
   */

  cbuf = (ChannelBuffer*) Tcl_Alloc (CHANNELBUFFER_HEADER_SIZE + toWrite);
  cbuf->nextAdded   = toWrite;
  cbuf->nextRemoved = 0;
  cbuf->bufLength   = toWrite;
  cbuf->nextPtr     = chan->last;

  memcpy ((VOID*) ((char*) cbuf->buf, (VOID*) buf, toWrite);

  if (chan->first == (ChannelBuffer*) NULL) {
    chan->first = cbuf;
  }

  chan->last   = cbuf;
  chan->length += toWrite;

  return toWrite;
}

/*
 *------------------------------------------------------*
 *
 *	GetOption --
 *
 *	------------------------------------------------*
 *	Computes an option value for a in-memory fifo
 *	channel, or a list of all options and their values.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		A standard Tcl result. The value of the
 *		specified option or a list of all options
 *		and their values is returned in the
 *		supplied DString.
 *
 *------------------------------------------------------*
 */

#if (TCL_MAJOR_VERSION < 8)
static int
GetOption (instanceData, optionName, dsPtr)
ClientData   instanceData;	/* Channel to query */
char*        optionName;	/* Name of reuqested option */
Tcl_DString* dsPtr;		/* String to place the result into */
#else
static int
GetOption (instanceData, interp, optionName, dsPtr)
ClientData   instanceData;	/* Channel to query */
Tcl_Interp*  interp;		/* Interpreter to leave error messages in */
char*        optionName;	/* Name of reuqested option */
Tcl_DString* dsPtr;		/* String to place the result into */
#endif
{
  /*
   * In-memory fifo channels provide a channel type specific,
   * read-only, fconfigure option, "length", that obtains
   * the current number of bytes of data stored in the channel.
   */

  ChannelInstance* chan;
  char             buffer [50];
  /* sufficient even for 64-bit quantities */

  chan = (ChannelInstance*) instanceData;

  if ((optionName != (char*) NULL) && (0 != strcmp (optionName, "-length"))) {
    Tcl_SetErrno (EINVAL);
#if (TCL_MAJOR_VERSION < 8)
    return TCL_ERROR;
#else
    return Tcl_BadChannelOption (interp, optionName, "length");
#endif
  }

  if (optionName == (char*) NULL) {
    /*
     * optionName == NULL
     * => a list of options and their values was requested,
     * so append the optionName before the retrieved value.
     */
    Tcl_DStringAppendElement (dsPtr, "-length");
  }

  sprintf (buffer, "%lu", chan->length);
  Tcl_DStringAppendElement (dsPtr, buffer);

  return TCL_OK;
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
#if (TCL_MAJOR_VERSION >= 8)
  /*
   * In-memory fifo channels are not based on files.
   * They are always writable, and almost always readable.
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
#else
  /*
   * In-memory fifo channels are not based on files. Readiness
   * for events is defined explicitly, see "ChannelReady".
   * Nothing has to be done here.
   */
#endif
}

#if (TCL_MAJOR_VERSION < 8)
/*
 *------------------------------------------------------*
 *
 *	ChannelReady --
 *
 *	------------------------------------------------*
 *	Called by the notifier to check whether events
 *	of interest are present on the channel.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		Returns OR-ed combination of TCL_READABLE,
 *		TCL_WRITABLE and TCL_EXCEPTION to indicate
 *		which events of interest are present.
 *
 *------------------------------------------------------*
 */

static int
ChannelReady (instanceData, mask)
ClientData instanceData;	/* Channel to query */
int        mask;		/* Mask of queried events */
{
  /*
   * In-memory fifo channels are always writable (fileevent
   * writable will fire continuously) and they are readable
   * if they are not empty.
   */

  ChannelInstance* chan;
  int              resultMask;

  chan       = (ChannelInstance*) instanceData;
  resultMask = mask;

  if ((resultMask & TCL_READABLE) && (! FIFO_EMPTY (chan))) {
      resultMask &= ~TCL_READABLE;
  }

  resultMask &= ~TCL_EXCEPTION;

  return resultMask;
}
#else
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
   * In-memory fifo channels are always writable (fileevent
   * writable) and they are readable if they are not empty.
   */

  ChannelInstance* chan = (ChannelInstance*) instanceData;
  int              mask = TCL_READABLE | TCL_WRITABLE;

  /*
   * Timer fired, our token is useless now.
   */

  chan->timer = (Tcl_TimerToken) NULL;

  if (! FIFO_EMPTY (chan)) {
    mask &= ~TCL_READABLE;
  }

  /* Tell Tcl about the possible events.
   * This will regenerate the timer too, via 'WatchChannel'.
   */

  Tcl_NotifyChannel (chan->chan, mask);
}
#endif

#if (TCL_MAJOR_VERSION < 8)
/*
 *------------------------------------------------------*
 *
 *	GetFile --
 *
 *	------------------------------------------------*
 *	Called from Tcl_GetChannelFile to retrieve
 *	Tcl_Files from inside a in-memory fifo channel.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		The appropriate Tcl_File or NULL if not
 *		present. 
 *
 *------------------------------------------------------*
 */
	/* ARGSUSED */
static Tcl_File
GetFile (instanceData, mask)
ClientData instanceData;	/* Channel to query */
int        mask;		/* Direction of interest */
{
  /*
   * In-memory fifo channels are not based on files.
   */

  return (Tcl_File) NULL;
}
#else
/*
 *------------------------------------------------------*
 *
 *	GetFile --
 *
 *	------------------------------------------------*
 *	Called from Tcl_GetChannelHandle to retrieve
 *	OS handles from inside a in-memory fifo channel.
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
   * In-memory fifo channels are not based on files.
   */

  /* *handlePtr = (ClientData) NULL; */
  return TCL_ERROR;
}
#endif /* (TCL_MAJOR_VERSION < 8) */

/*
 *------------------------------------------------------*
 *
 *	MemchanFifoCmd --
 *
 *	------------------------------------------------*
 *	This procedure realizes the 'fifo' command.
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
#if TCL_MAJOR_VERSION < 8
MemchanFifoCmd (notUsed, interp, argc, argv)
ClientData  notUsed;		/* Not used. */
Tcl_Interp* interp;		/* Current interpreter. */
int         argc;		/* Number of arguments. */
char**      argv;		/* Argument strings. */
#else
MemchanFifoCmd (notUsed, interp, objc, objv)
ClientData  notUsed;		/* Not used. */
Tcl_Interp* interp;		/* Current interpreter. */
int         objc;		/* Number of arguments. */
Tcl_Obj**   objv;		/* Argument objects. */
#endif
{
  Tcl_Channel      chan;
  ChannelInstance* instance;

#if TCL_MAJOR_VERSION < 8
  char* channelHandle;
#else
  Tcl_Obj* channelHandle;
#endif

  if (argc != 1) {
    Tcl_AppendResult (interp,
		      "wrong # args: should be \"fifo\"",
		      (char*) NULL);
    return TCL_ERROR;
  }

  instance = (ChannelInstance*) Tcl_Alloc (sizeof (ChannelInstance));
  instance->length = 0;
  instance->first  = (ChannelBuffer) NULL;
  instance->last   = (ChannelBuffer) NULL;

  channelHandle = MemchanGenHandle ("fifo");

#if TCL_MAJOR_VERSION < 8
  chan = Tcl_CreateChannel (&channelType,
			    channelHandle,
			    (ClientData) instance,
			    TCL_READABLE | TCL_WRITABLE);
#else
  chan = Tcl_CreateChannel (&channelType,
			    Tcl_GetStringFromObj (channelHandle, NULL),
			    (ClientData) instance,
			    TCL_READABLE | TCL_WRITABLE);
#endif

  instance->chan      = chan;

#if (TCL_MAJOR_VERSION >= 8)
  instance->timer     = (Tcl_TimerToken) NULL;
#endif

  Tcl_RegisterChannel  (interp, chan);
  Tcl_SetChannelOption (interp, chan, "-buffering", "none");
  Tcl_SetChannelOption (interp, chan, "-blocking",  "0");

#if TCL_MAJOR_VERSION < 8
  Tcl_AppendResult     (interp, channelHandle, (char*) NULL);
#else
  Tcl_SetObjResult     (interp, channelHandle);
#endif
  return TCL_OK;
}

