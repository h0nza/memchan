/*
 * random.c --
 *
 *	Implementation of a random Tcl file channel
 *
 *  The PRNG in use here is the ISAAC PRNG. See
 *    http://www.burtleburtle.net/bob/rand/isaacafa.html
 *  for details. This generator _is_ suitable for cryptographic use
 *
 * Copyright (C) 2004 Pat Thoyts <patthoyts@users.sourceforge.net>
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


#include "memchanInt.h"
#include "../isaac/rand.h"
#include <time.h>
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

/*
 * This structure describes the channel type structure for random channels:
 * random channels are not seekable. They have no options.
 */

static Tcl_ChannelType channelType = {
    "random",			/* Type name.                                */
    NULL,			/* Set blocking/nonblocking.       NULL'able */
    Close,			/* Close channel, clean instance data        */
    Input,			/* Handle read request                       */
    Output,			/* Handle write request                      */
    NULL,			/* Move location of access point.  NULL'able */
    NULL,			/* Set options.                    NULL'able */
    NULL,			/* Get options.                    NULL'able */
    WatchChannel,		/* Initialize notifier                       */
#if GT81
    GetFile,			/* Get OS handle from the channel.           */
    NULL			/* Close2Proc, not available, no partial close
				 * possible */
#else
    GetFile			/* Get OS handle from the channel.           */
#endif
};


/*
 * This structure describes the per-instance state of a in-memory null channel.
 */

typedef struct ChannelInstance {
    Tcl_Channel    chan;   /* Backreference to generic channel information */
    Tcl_TimerToken timer;  /* Timer used to link the channel into the
			    * notifier. */
    struct randctx state;  /* PRNG state */
} ChannelInstance;

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
     ClientData  instanceData;	/* The instance information of the channel to
				 * close */
     Tcl_Interp* interp;	/* unused */
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
     char*      buf;		/* Buffer to fill */
     int        toRead;		/* Requested number of bytes */
     int*       errorCodePtr;	/* Location of error flag */
{
    ChannelInstance *chan = (ChannelInstance *)instanceData;    
    size_t n = 0, i = sizeof(unsigned long);
    unsigned long rnd;

    for (n = 0; toRead - n > i; n += i) {
	rnd = rand(&chan->state);
	memcpy(&buf[n], (char *)&rnd, i);
    }
    if (toRead - n > 0) {
	rnd = rand(&chan->state);
	memcpy(&buf[n], (char *)&rnd, toRead-n);
	n += (toRead-n);
    }

    return n;
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
     int        toWrite;	/* Number of bytes to write. */
     int*       errorCodePtr;	/* Location of error flag. */
{
    ChannelInstance *chan = (ChannelInstance *)instanceData;    
    ub4 rnd, n = 0;
    ub4 *s = (ub4 *)buf;
    ub4 *p = chan->state.randrsl;

    while (n < RANDSIZ && n < (ub4)(toWrite/4)) {
	p[n] ^= s[n]; n++;
    }
    /* mix the state */
    rnd = rand(&chan->state);

    /* 
     * If we filled the state with data, there is no advantage to
     * adding in additional data. So lets save time.
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
	chan->timer = Tcl_CreateTimerHandler(DELAY, ChannelReady, 
	    instanceData);
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
     ClientData instanceData;	/* Channel to query */
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
     int         direction;	/* Direction of interest */
     ClientData* handlePtr;	/* Space to the handle into */
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
 *	MemchanRandomCmd --
 *
 *	------------------------------------------------*
 *	This procedure realizes the 'random' command.
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
MemchanRandomCmd (notUsed, interp, objc, objv)
     ClientData    notUsed;		/* Not used. */
     Tcl_Interp*   interp;		/* Current interpreter. */
     int           objc;		/* Number of arguments. */
     Tcl_Obj*CONST objv[];		/* Argument objects. */
{
    Tcl_Obj*         channelHandle;
    Tcl_Channel      chan;
    ChannelInstance* instance;
    unsigned long seed;
    
    if (objc != 1) {
	Tcl_WrongNumArgs(interp, 0, objv, "");
	return TCL_ERROR;
    }
    
    instance      = (ChannelInstance*) Tcl_Alloc (sizeof (ChannelInstance));
    channelHandle = MemchanGenHandle ("random");
    
    chan = Tcl_CreateChannel (&channelType,
	Tcl_GetStringFromObj (channelHandle, NULL),
	(ClientData) instance,
	TCL_READABLE | TCL_WRITABLE);
    
    instance->chan      = chan;
    instance->timer     = (Tcl_TimerToken) NULL;

    /*
     * Basic initialization of the PRNG state
     */
    seed = time(NULL) + ((long)Tcl_GetCurrentThread() << 12);
    memcpy(&instance->state.randrsl, &seed, sizeof(seed));
    randinit(&instance->state);
    
    Tcl_RegisterChannel  (interp, chan);
    Tcl_SetChannelOption (interp, chan, "-buffering", "none");
    Tcl_SetChannelOption (interp, chan, "-blocking",  "0");
    
    Tcl_SetObjResult     (interp, channelHandle);
    return TCL_OK;
}
