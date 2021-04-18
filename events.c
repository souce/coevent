/*
 * 
 * Copyright (c) 2021, Joel
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "events.h"

events *events_create(int setsize){
    if(0 < setsize){
        return aeCreateEventLoop(setsize);
    }
    return NULL;
}

void events_run(events *loop){
    if(NULL != loop){
        aeMain(loop);
    }
}

void events_stop(events *loop){
    if(NULL != loop){
        aeStop(loop);
    }
}

void events_free(events *loop){
    if(NULL != loop){
        aeDeleteEventLoop(loop);
    }
}


////////////////////////////////////////////////////////////////////////////////
// Timer event callback
////////////////////////////////////////////////////////////////////////////////
long long events_create_timer(events *loop, long long milliseconds, events_timer_proc *proc, void *data){
    if(NULL != loop && NULL != proc){
        return aeCreateTimeEvent(loop, milliseconds, proc, data, NULL);
    }
    return -1;
}

int events_delete_timer(events *loop, long long id){
    if(NULL != loop){
        return aeDeleteTimeEvent(loop, id);
    }
    return EVENTS_ERR;
}


////////////////////////////////////////////////////////////////////////////////
// IO event callback
////////////////////////////////////////////////////////////////////////////////
int events_create_io(events *loop, int fd, int mask, events_file_proc *proc, void *data){
    if(NULL != loop && 0 <= fd && AE_NONE != mask && NULL != proc){
        return aeCreateFileEvent(loop, fd, mask, proc, data);
    }
    return EVENTS_ERR;
}

void events_remove_io_mask(events *loop, int fd, int mask){
    if(NULL != loop && 0 <= fd && AE_NONE != mask){
        aeDeleteFileEvent(loop, fd, mask);
    }
}

int events_get_io_mask(events *loop, int fd){
    if(NULL != loop && 0 <= fd){
        return aeGetFileEvents(loop, fd);
    }
    return AE_NONE; //0
}


////////////////////////////////////////////////////////////////////////////////
// IO events with Timeout
////////////////////////////////////////////////////////////////////////////////
static int time_proc(events *loop, long long id, void *data){
    events_once_event *once = (events_once_event *)data;
    events_once_proc *proc = once->proc;
    void *clientData = once->clientData;
    int event = EVENTS_ERR;
    if(0 <= once->io_fd){
        //a timer callback related to IO, which means that this is an IO timeout event
        events_remove_io_mask(loop, once->io_fd, EVENTS_READABLE|EVENTS_WRITABLE);
        event = EVENTS_TIMEOUT;
    }else{
        //time is up
        event = EVENTS_TIMEUP;
    }
    free(once);
    proc(loop, event, clientData);
    return EVENTS_NOMORE;  //the timer callback is disposable
}

static void file_proc(events *loop, int fd, void *data, int event){
    events_once_event *once = (events_once_event *)data;
    events_once_proc *proc = once->proc;
    void *clientData = once->clientData;
    events_remove_io_mask(loop, fd, (EVENTS_READABLE|EVENTS_WRITABLE)); //the IO callback is disposable
    events_delete_timer(loop, once->timer_id); //stop the timeout event corresponding to the IO event
    free(once);
    proc(loop, event, clientData);
}

/*
 * According to different parameters, the registered events are different:
 *     1. If FD is not specified, it is a "one-time event";
 *     2. If the time is not specified, it is "one time IO event";
 *     3. Specify FD and time as "one time IO event with timeout";
 * 
 * WARNING: if neither fd or time is specified, the operation fails!
 * WARNING: the same fd cannot bind IO events more than once!
 */
int events_create_once(events *loop, int fd, int mask, int time, events_once_proc *proc, void *data){
    if(NULL == loop || NULL == proc){
        return EVENTS_ERR;
    }

    events_once_event *once = calloc(1, sizeof(events_once_event));
    if(NULL == once){
        return EVENTS_ERR;
    }
    once->io_fd = fd;
    once->proc = proc;
    once->clientData = data;

    if(0 <= fd){
        if(AE_NONE != events_get_io_mask(loop, fd)){
            return EVENTS_ERR;  //WARNING: the same fd cannot bind IO events more than once!
        }

        if(!(mask&(EVENTS_READABLE|EVENTS_WRITABLE))){
            return EVENTS_ERR;
        }

        if(EVENTS_ERR == events_create_io(loop, fd, mask, 
                                       file_proc, (void *)once)){
            return EVENTS_ERR;
        }
    }

    if(0 <= time){
        if(EVENTS_ERR == (once->timer_id = events_create_timer(loop, time, time_proc, (void *)once))){
            return EVENTS_ERR;
        }
    }

    if(0 > fd && 0 > time){
        return EVENTS_ERR;  //WARNING: if neither fd or time is specified, the operation fails!
    }
    return EVENTS_OK;
}
