# coevent

coevent is a coroutine library for C, based on ["ae"](https://github.com/redis/redis/blob/unstable/src/ae.c) and ["pt"](http://dunkels.com/adam/pt/), the coroutine scheduling by IO events.

no more callback hell.

example (the source code of HTTP server is not included in this repository) :

```c
/*
 * clean up after the coroutine end
 */
static void ce_done(struct coevent *ce, int err){
    if(NULL == ce){
        return; //relax take it easy, for there is nothing that we can do
    }

    if(AE_ERR == err){
        perror("connection err");
    }else if(EVENTS_TIMEOUT == err){
        perror("connection timeout");
    }else if(0 != err){
        fprintf(stderr, "unknown err:%d (%s)\n", errno, strerror(errno));
    }

    if(NULL != ce->priv){
        http_free_client((struct http_client *)(ce->priv));
    }
    COEVENT_FREE(ce);
}

/*
 * handling the HTTP request
 */
static COEVENT_COROUTINE(ce_process(struct coevent *ce)){
    struct http_client *cli = (struct http_client *)(ce->priv);

    //define local variables for coroutines
    struct{
        int retry;
    } *local_vars = COEVENT_GET_VARS(ce, sizeof(*local_vars));
    if(NULL == local_vars){
        printf("init local variables err, errno:%d, client ip:%s", errno, cli->ip);
        goto over;
    }

    COEVENT_BEGIN(ce);

    //read HTTP request
    local_vars->retry = 0;
    while(MAX_RETRY_COUNT > local_vars->retry++){
        //wait for the read event to be triggered before proceeding
        COEVENT_YIELD_UNTIL(ce, cli->cli_fd, EVENTS_READABLE, MAX_WAIT_TIMEOUT);
        if(AIO_ERR == aio_read(cli->cli_fd, (byte *)cli->req_buf, sizeof(cli->req_buf), &(cli->readn))){
            break; //connection reset by peer, or other exception
        }
        if(HTTP_OK == http_parse_request_line(cli)){
            break; //request line read complete
        }
    }

    //check request parameters ...

    COEVENT_YIELD(ce); //immediately interrupt the coroutine function and return, and wait until the next event cycle

    //processing HTTP requests
    if(HTTP_ERR == http_service(RESOURCE_PATH, cli)){
        goto over;
    }

    COEVENT_YIELD(ce); //immediately interrupt the coroutine function and return, and wait until the next event cycle

    //write HTTP response
    local_vars->retry = 0;
    while(MAX_RETRY_COUNT > local_vars->retry++ && cli->res_len > cli->written){
        //try "write" once to speed up the response
        if(AIO_ERR == aio_write(cli->cli_fd, (byte *)cli->res_buf, cli->res_len, &(cli->written))){
            goto over;
        }
        COEVENT_YIELD_UNTIL(ce, cli->cli_fd, EVENTS_WRITABLE, MAX_WAIT_TIMEOUT); //wait for the event
    }
    //even if the response cannot be written, there is no need to try again
    errno = 0; //reset error code

over:
    //normal end or exception encountered
    COEVENT_END(ce, errno);
}

/*
 * accept the client connection, and start the coroutine
 */
static void handle_request(aeEventLoop *loop, int server_fd, void *data, int mask){
    int client_fd = 0;
    struct coevent *ce = NULL;
    struct http_client *cli = NULL;

    //...
    
    //accept the client connection
    client_fd = socket_handle_client(server_fd);
    if(0 >= client_fd){
        perror("handle clent");
        goto err;
    }
    cli = calloc(1, sizeof(*cli));
    if(NULL == cli){
        perror("create ctx failed!");
        goto err;
    }
    cli->cli_fd = client_fd;

    //initialize the coroutine
    ce = COEVENT_CREATE(loop, ce_process, ce_done, (void *)cli, STACK_SIZE);
    if(NULL == ce){
        perror("create ce failed in dispatcher!");
        goto err;
    }
    //start the coroutine and process the request
    COEVENT_RUN(ce);
    return;

err:
    if(0 < client_fd) close(client_fd);
    if(NULL != cli) free(cli);
    COEVENT_FREE(ce);
}
```

