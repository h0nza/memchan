
/*
 * memchan.c --
 *
 *	Implementation of a memory channel.
 *
 * Copyright (c) Sep 1996 Andreas Kupries (a.kupries@westend.com)
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
 * CVS: $Id$
 */


#include <tcl.h>
#include <errno.h>

/*
 * Number of bytes used to extend a storage area being to small.
 */

#define INCREMENT (512)


/*
 * Forward declarations of internal procedures.
 */

static int	Close _ANSI_ARGS_((ClientData instanceData, Tcl_Interp *interp));

static int	Input _ANSI_ARGS_((ClientData instanceData,
		    char *buf, int toRead, int *errorCodePtr));

static int	Output _ANSI_ARGS_((ClientData instanceData,
	            char *buf, int toWrite, int *errorCodePtr));

static int	Seek _ANSI_ARGS_((ClientData instanceData,
		    long offset, int mode, int *errorCodePtr));

static int	GetOption _ANSI_ARGS_((
		    ClientData instanceData, char *optionName,
                    Tcl_DString *dsPtr));

static void	WatchChannel _ANSI_ARGS_((ClientData instanceData, int mask));
static int	ChannelReady _ANSI_ARGS_((ClientData instanceData, int mask));
static Tcl_File GetFile      _ANSI_ARGS_((ClientData instanceData, int mask));


/*
 * This structure describes the channel type structure for in-memory channels:
 */

static Tcl_ChannelType channelType = {
  "memory",		/* Type name.                                    */
  NULL,			/* Set blocking/nonblocking behaviour. NULL'able */
  Close,		/* Close channel, clean instance data            */
  Input,		/* Handle read request                           */
  Output,		/* Handle write request                          */
  Seek,			/* Move location of access point.      NULL'able */
  NULL,			/* Set options.                        NULL'able */
  GetOption,		/* Get options.                        NULL'able */
  WatchChannel,		/* Initialize notifier                           */
  ChannelReady,		/* Are there events?                             */
  GetFile		/* Get Tcl_Files out of channel                  */
};


/*
 * This structure describes the per-instance state of a in-memory channel.
 */

typedef struct ChannelInstance {
  unsigned long rwLoc;	   /* current location to read from (or write to). */
  unsigned long allocated; /* number of allocated bytes */
  unsigned long used;	   /* number of bytes stored in the channel. */
  VOID*         data;	   /* memory plane used to store the channel contents */
} ChannelInstance;

/*
 *------------------------------------------------------*
 *
 *	Close --
 *
 *	------------------------------------------------*
 *	This procedure is called from the generic IO
 *	level to perform channel-type-specific cleanup
 *	when a in-memory channel is closed.
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
ClientData  instanceData;    /* The instance information of the channel to close */
Tcl_Interp* interp;          /* unused */
{
  ChannelInstance* chan;

  chan = (ChannelInstance*) instanceData;

  Tcl_Free ((char*) chan->data);
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
 *	level to read input from a in-memory channel.
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
  ChannelInstance* chan;

  if (toRead == 0)
    return 0;

  chan = (ChannelInstance*) instanceData;

  if ((chan->rwLoc + toRead) > chan->used) {
    /*
     * Reading behind the last byte is not possible,
     * truncate the request.
     */
    toRead = chan->used - chan->rwLoc;
  }

  if (toRead > 0) {
    memcpy ((VOID*) buf, (VOID*) ((char*) chan->data + chan->rwLoc), toRead);
    chan->rwLoc += toRead;
  }

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

  if (toWrite == 0)
    return 0;

  chan = (ChannelInstance*) instanceData;

  if ((chan->rwLoc + toWrite) > chan->allocated) {
    /*
     * We are writing beyond the end of the allocated area.
     * It is necessary to extend the it. Try to use a fixed
     * increment first and adjust if that is not enough.
     */

    chan->allocated += INCREMENT;

    if ((chan->rwLoc + toWrite) > chan->allocated) {
      chan->allocated = chan->rwLoc + toWrite;
    }

    chan->data = Tcl_Realloc (chan->data, chan->allocated);
  }

  memcpy ((VOID*) ((char*) chan->data + chan->rwLoc), (VOID*) buf, toWrite);
  chan->rwLoc += toWrite;

  if (chan->rwLoc > chan->used)
    chan->used = chan->rwLoc;

  return toWrite;
}

/*
 *------------------------------------------------------*
 *
 *	Seek --
 *
 *	------------------------------------------------*
 *	This procedure is called by the generic IO level
 *	to move the access point in a in-memory channel.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Moves the location at which the channel
 *		will be accessed in future operations.
 *
 *	Result:
 *		-1 if failed, the new position if
 *		successful. An output argument contains
 *		the POSIX error code if an error
 *		occurred, or zero.
 *
 *------------------------------------------------------*
 */

static int
Seek (instanceData, offset, mode, errorCodePtr)
ClientData instanceData;	/* The channel to manipulate */
long       offset;		/* Size of movement. */
int        mode;		/* How to move */
int*       errorCodePtr;	/* Location of error flag. */
{
  ChannelInstance* chan;
  int              newLocation;

  chan = (ChannelInstance*) instanceData;
  *errorCodePtr = 0;

  switch (mode) {
  case SEEK_SET:
    newLocation = offset;
    break;

  case SEEK_CUR:
    newLocation = chan->rwLoc + offset;
    break;

  case SEEK_END:
    newLocation = chan->used - offset;
    break;

  default:
    panic ("illegal seek-mode specified");
    return -1;
  }

  if ((newLocation < 0) || (newLocation > chan->used)) {
    *errorCodePtr = EINVAL; /* EBADRQC ?? */
    return -1;
  }

  chan->rwLoc = newLocation;

  return newLocation;
}

/*
 *------------------------------------------------------*
 *
 *	GetOption --
 *
 *	------------------------------------------------*
 *	Computes an option value for a in-memory channel,
 *	or a list of all options and their values.
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

static int
GetOption (instanceData, optionName, dsPtr)
ClientData   instanceData;	/* Channel to query */
char*        optionName;	/* Name of reuqested option */
Tcl_DString* dsPtr;		/* String to place the result into */
{
  /*
   * In-memory channels provide a channel type specific,
   * read-only, fconfigure option, "length", that obtains
   * the current number of bytes of data stored in the channel.
   */

  ChannelInstance* chan;
  char             buffer [50];
  /* sufficient even for 64-bit quantities */

  chan = (ChannelInstance*) instanceData;

  if ((optionName != (char*) NULL) && (0 != strcmp (optionName, "-length"))) {
    Tcl_SetErrno (EINVAL);
    return TCL_ERROR;
  }


  if (optionName == (char*) NULL) {
    /*
     * optionName == NULL
     * => a list of options and their values was requested,
     * so append the optionName before the retrieved value.
     */
    Tcl_DStringAppendElement (dsPtr, "-length");
  }

  sprintf (buffer, "%lu", chan->used);
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
  /*
   * In-memory channels are not based on files. Readiness
   * for events is defined explicitly, see "ChannelReady".
   * Nothing has to be done here.
   */
}

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
   * In-memory channels are always writable (fileevent
   * writable will fire continuously) and they are readable
   * when the current access point is before the last byte
   * contained in the channel.
   */

  ChannelInstance* chan;
  int              resultMask;

  chan       = (ChannelInstance*) instanceData;
  resultMask = mask;

  if ((resultMask & TCL_READABLE) &&
      (chan->rwLoc >= chan->used)) {
      resultMask &= ~TCL_READABLE;
  }

  resultMask &= ~TCL_EXCEPTION;

  return resultMask;
}

/*
 *------------------------------------------------------*
 *
 *	GetFile --
 *
 *	------------------------------------------------*
 *	Called from Tcl_GetChannelFile to retrieve
 *	Tcl_Files from inside a in-memory channel.
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
   * In-memory channel are not based on files.
   */

  return (Tcl_File) NULL;
}

/*
 *------------------------------------------------------*
 *
 *	MemoryChannelCmd --
 *
 *	------------------------------------------------*
 *	This procedure realizes the 'memchan' command.
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
MemoryChannelCmd (notUsed, interp, argc, argv)
ClientData  notUsed;		/* Not used. */
Tcl_Interp* interp;		/* Current interpreter. */
int         argc;		/* Number of arguments. */
char**      argv;		/* Argument strings. */
{
  Tcl_Channel      chan;
  ChannelInstance* instance;
  char             channelName [50];

  /*
   * count number of generated memory channels,
   * used for id generation. Ids are never reclaimed
   * and there is no dealing with wrap around. On the
   * other hand, "unsigned long" should be big enough
   * except for absolute longrunners (generate 100 ids
   * per second => overflow will occur in 1 1/3 years).
   */

  static unsigned long memCounter = 0;


  if (argc != 1) {
    Tcl_AppendResult (interp,
		      "wrong # args: should be \"memchan\"",
		      (char*) NULL);
    return TCL_ERROR;
  }

  instance            = (ChannelInstance*) Tcl_Alloc (sizeof (ChannelInstance));
  instance->rwLoc     = 0;
  instance->allocated = 0;
  instance->used      = 0;
  instance->data      = (VOID*) NULL;

  sprintf (channelName, "mem%lu", memCounter);
  memCounter ++;

  chan = Tcl_CreateChannel (&channelType,
			    channelName,
			    instance,
			    TCL_READABLE | TCL_WRITABLE);

  Tcl_RegisterChannel  (interp, chan);
  Tcl_SetChannelOption (interp, chan, "-buffering", "none");
  Tcl_AppendResult     (interp, channelName, (char*) NULL);
  return TCL_OK;
}

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
 *		As of 'MemGetRegistry'.
 *
 *	Result:
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

EXTERN int
Memchan_Init (interp)
Tcl_Interp* interp;
{
  Tcl_CreateCommand (interp, "memchan", &MemoryChannelCmd,
		     (ClientData) NULL, NULL);

  /* register memory channels as now available package */
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

EXTERN int
Mem_SafeInit (interp)
Tcl_Interp* interp;
{
  return Memchan_Init (interp);
}

