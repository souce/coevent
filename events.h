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

#ifndef __EVENTS_H_
#define __EVENTS_H_
#ifdef __cplusplus
extern "C" {
#endif

    #include "ae.h"  //the Redis ae event loop
    
    #define EVENTS_OK AE_OK
    #define EVENTS_ERR AE_ERR

    #define EVENTS_NONE AE_NONE
    #define EVENTS_NOMORE AE_NOMORE
    #define EVENTS_READABLE AE_READABLE
    #define EVENTS_WRITABLE AE_WRITABLE
    #define EVENTS_TIMEOUT 4 //timeout, something wrong
    #define EVENTS_TIMEUP 8  //timeup, everything is fine

    typedef aeEventLoop events;

    events *events_create(int setsize);
    void events_run(events *loop);
    void events_stop(events *loop);
    void events_free(events *loop);

    //Timer event
    typedef aeTimeProc events_timer_proc;
    long long events_create_timer(events *loop, long long milliseconds, events_timer_proc *proc, void *data);
    int events_delete_timer(events *loop, long long id);

    //IO event
    typedef aeFileProc events_file_proc;
    int events_create_io(events *loop, int fd, int mask, events_file_proc *proc, void *data);
    void events_remove_io_mask(events *loop, int fd, int mask);
    int events_get_io_mask(events *loop, int fd);

    //IO events with Timeout
    typedef void events_once_proc(events *loop, int event, void *data);
    typedef struct events_once_event {
        int io_fd;          //The file handle corresponding to the IO event
        long long timer_id; //Timer corresponding to IO event
        events_once_proc *proc;
        void *clientData;
    } events_once_event;
    int events_create_once(events *loop, int fd, int event, int time, events_once_proc *proc, void *data);

#ifdef __cplusplus
}
#endif
#endif

