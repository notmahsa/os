#include "request.h"
#include "server_thread.h"
#include "common.h"
#include <pthread.h>

struct server {
	int nr_threads;
	int max_requests;
	int max_cache_size;
	int exiting;
	/* add any other parameters you need */
	pthread_t ** worker_threads;
	int * req_queue;
	pthread_mutex_t * lock;
    pthread_cond_t * full;
    pthread_cond_t * empty;

    int buff_low;
    int buff_high;

};

void request_stub(void *sv);

/* static functions */

/* initialize file data */
static struct file_data *
file_data_init(void)
{
	struct file_data *data;

	data = Malloc(sizeof(struct file_data));
	data->file_name = NULL;
	data->file_buf = NULL;
	data->file_size = 0;
	return data;
}

/* free all file data */
static void
file_data_free(struct file_data *data)
{
	free(data->file_name);
	free(data->file_buf);
	free(data);
}

static void
do_server_request(struct server *sv, int connfd)
{
	int ret;
	struct request *rq;
	struct file_data *data;

	data = file_data_init();

	/* fill data->file_name with name of the file being requested */
	rq = request_init(connfd, data);
	if (!rq) {
		file_data_free(data);
		return;
	}
	/* read file,
	 * fills data->file_buf with the file contents,
	 * data->file_size with file size. */
	ret = request_readfile(rq);
	if (ret == 0) { /* couldn't read file */
		goto out;
	}
	/* send file to client */
	request_sendfile(rq);
out:
	request_destroy(rq);
	file_data_free(data);
}

/* entry point functions */

struct server *
server_init(int nr_threads, int max_requests, int max_cache_size)
{
	struct server *sv;

	sv = Malloc(sizeof(struct server));
	sv->nr_threads = nr_threads;
	sv->max_requests = max_requests;
	sv->max_cache_size = max_cache_size;
	sv->exiting = 0;

	int err;

	if (nr_threads > 0 || max_requests > 0 || max_cache_size > 0) {
	    sv->lock = malloc(sizeof(pthread_mutex_t));
	    err = pthread_mutex_init(sv->lock, NULL);
	    assert(err == 0);

	    sv->full = malloc(sizeof(pthread_cond_t));
	    err = pthread_cond_init(sv->full, NULL);
	    assert(err == 0);

	    sv->empty = malloc(sizeof(pthread_cond_t));
        err = pthread_cond_init(sv->empty, NULL);
        assert(err == 0);

        sv->buff_low = 0;
        sv->buff_high = 0;

		if (max_requests > 0){
		    sv->req_queue = malloc(max_requests * sizeof(int));
		}

		if (nr_threads > 0){
		    sv->worker_threads = malloc(nr_threads * sizeof(pthread_t *));
		    for (int i = 0; i < nr_threads; i++){
            	err = pthread_create(sv->worker_threads[i], NULL, request_stub, sv);
            	assert(err == 0);
            }
		}
	}

	/* Lab 4: create queue of max_request size when max_requests > 0 */

	/* Lab 5: init server cache and limit its size to max_cache_size */

	/* Lab 4: create worker threads when nr_threads > 0 */

	return sv;
}

void
request_stub(void * sv_void){
    int connfd = 0;
    struct server * sv = (struct server *)sv_void;

    while(1){
        pthread_mutex_lock(sv->lock);

        while((sv->buff_high - sv->buff_low + sv->max_requests) % sv->max_requests == 0){
            pthread_cond_wait(sv->empty, sv->lock);
        }

        connfd = sv->req_queue[sv->buff_low];
        sv->req_queue[sv->buff_low] = 0;
        if (sv->buff_low == sv->max_requests - 1){
            sv->buff_low = 0;
        } else {
            sv->buff_low++;
        }

        pthread_cond_signal(sv->empty);

        pthread_mutex_unlock(sv->lock);
        do_server_request(sv, connfd);
    }
}

void
server_request(struct server *sv, int connfd)
{
	if (sv->nr_threads == 0) { /* no worker threads */
		do_server_request(sv, connfd);
	} else {
		/*  Save the relevant info in a buffer and have one of the
		 *  worker threads do the work. */

		pthread_mutex_lock(sv->lock);
        if ((sv->buff_high - sv->buff_low + sv->max_requests) % sv->max_requests == sv->max_requests - 1){
            pthread_cond_wait(sv->empty, sv->lock);
        }

        sv->req_queue[sv->buff_high] = connfd;

        if (sv->buff_high == sv->max_requests - 1){
            sv->buff_high = 0;
        } else{
            sv->buff_high++;
        }

        pthread_cond_signal(sv->empty);
        pthread_mutex_unlock(sv->lock);
	}
}

void
server_exit(struct server *sv)
{
	/* when using one or more worker threads, use sv->exiting to indicate to
	 * these threads that the server is exiting. make sure to call
	 * pthread_join in this function so that the main server thread waits
	 * for all the worker threads to exit before exiting. */
	sv->exiting = 1;

	/* make sure to free any allocated resources */
	free(sv);
}
