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

#ifndef __COEVENT_H__
#define __COEVENT_H__
#ifdef __cplusplus
extern "C" {
#endif

    #include "pt.h"
    #include "events.h"

    #define COEVENT_COROUTINE(name_args) void (name_args)

    #define COEVENT_CREATE(eloop,ceproc,ceover,cepriv,ssize) \
        ({ \
            size_t aligned_size = (((ssize) + sizeof(void *) - 1) & ~(sizeof(void *) - 1)); \
            struct coevent *new_ce = (struct coevent *)calloc(1, sizeof(*new_ce) + aligned_size); \
            if(NULL != new_ce){ \
                PT_INIT(new_ce); \
                new_ce->loop = (eloop); \
                new_ce->proc = (ceproc); \
                new_ce->over = (ceover); \
                new_ce->priv = (cepriv); \
                new_ce->stack_size = aligned_size; \
            } \
            new_ce; \
        })
    
    #define COEVENT_RUN(ce) \
        do{ \
            if(NULL != (ce)){ \
                (ce)->proc(ce); \
            } \
        }while(0);
    
    #define COEVENT_FREE(ce) \
        do{ \
            if(NULL != (ce)){ \
                free((ce)); \
            } \
        }while(0);

    #define COEVENT_BEGIN(ce) PT_BEGIN(ce)

    #define COEVENT_GET_VARS(ce,ssize) \
        ({ \
            void *stack = NULL; \
            if(NULL != (ce) && (ssize) <= (ce)->stack_size){ \
                stack = (void *)(ce)->stack; \
            } \
            stack; \
        })

    #define COEVENT_END(ce,err) \
            PT_INIT(ce); \
            LC_END((ce)->lc); \
            (ce)->over((ce), (err)); \
            return; \
        }
    
    /*
     * Immediately interrupt the coroutine function and return, 
     * if the event registration fails, or the IO event is triggered, or the event times out, 
     * then resume execution of the coroutine.
     * 
     * WARNING: the same fd cannot bind IO events more than once!
     */
    #define COEVENT_YIELD_UNTIL(ce,fd,mask,timeout) \
        do{ \
            PT_YIELD_FLAG = 0; \
            LC_SET((ce)->lc); \
            if(PT_YIELD_FLAG == 0) { \
                int res = events_create_once((ce)->loop, (fd), (mask), (timeout), (ev_callback), (void *)(ce)); \
                if(EVENTS_OK != res) { \
                    ev_callback((ce)->loop, EVENTS_ERR, (void *)(ce)); \
                } \
                return; \
            } \
        }while(0);

    /*
     * Immediately interrupt the coroutine function and return, 
     * and does not continue to execute until the next event loop.
     */
    #define COEVENT_YIELD(ce) \
        do{ \
            PT_YIELD_FLAG = 0; \
            LC_SET((ce)->lc); \
            if(PT_YIELD_FLAG == 0) { \
                int res = events_create_once((ce)->loop, -1, 0, 0, (ev_callback), (void *)(ce)); \
                if(EVENTS_OK != res) { \
                    ev_callback((ce)->loop, EVENTS_ERR, (void *)(ce)); \
                } \
                return; \
            } \
        }while(0);

    /*
     * Immediately interrupt the coroutine function and return, 
     * wait for a while and then resume execution of the coroutine.
     */
    #define COEVENT_YIELD_WAIT(ce,time) \
        do{ \
            PT_YIELD_FLAG = 0; \
            LC_SET((ce)->lc); \
            if(PT_YIELD_FLAG == 0) { \
                int res = events_create_once((ce)->loop, -1, 0, (time), (ev_callback), (void *)(ce)); \
                if(EVENTS_OK != res) { \
                    ev_callback((ce)->loop, EVENTS_ERR, (void *)(ce)); \
                } \
                return; \
            } \
        }while(0);

    struct coevent;
    typedef COEVENT_COROUTINE(coevent_proc(struct coevent *ce));
    typedef void coevent_over(struct coevent *ce, int event);

    /*
     * the context of the coroutine
     */
    struct coevent{
        lc_t lc;            //line number counter
        events *loop;
        coevent_proc *proc; //coroutine callback
        coevent_over *over; //end or exception callback
        void *priv;
        size_t stack_size;
        char stack[0];      //the stack of the coroutine
    };

    /*
     * IO and Timer shared event callback
     */
    static void ev_callback(events *loop, int event, void *data) __attribute__((unused));
    static void ev_callback(events *loop, int event, void *data){
        struct coevent *ce = (struct coevent *)data;
        if(event&(EVENTS_READABLE|EVENTS_WRITABLE|EVENTS_TIMEUP)){
            ce->proc(ce); //resume coroutine
        }else{ 
            ce->over(ce, event); //something wrong: EVENTS_ERR, EVENTS_TIMEOUT or others
        }
    }

#ifdef __cplusplus
}
#endif
#endif
