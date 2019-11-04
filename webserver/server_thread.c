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
	int * request_buff;
	pthread_mutex_t * lock;
    pthread_cond_t * full;
    pthread_cond_t * empty;

    int buff_in;
    int buff_out;

};

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
void
request_stub(void * sv_void);

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
	    sv->lock = (pthread_mutex_t *)Malloc(sizeof(pthread_mutex_t));
	    err = pthread_mutex_init(sv->lock, NULL);
	    assert(err == 0);

	    sv->full = (pthread_cond_t *)Malloc(sizeof(pthread_cond_t));
	    err = pthread_cond_init(sv->full, NULL);
	    assert(err == 0);

	    sv->empty = (pthread_cond_t *)Malloc(sizeof(pthread_cond_t));
        err = pthread_cond_init(sv->empty, NULL);
        assert(err == 0);

        sv->buff_in = 0;
        sv->buff_out = 0;

		if (max_requests > 0){
		    sv->request_buff = (int *)Malloc((max_requests) * sizeof(int));
		    for (int i = 0; i < max_requests; i++){
		        sv->request_buff[i] = 0;
		    }
		}

		if (nr_threads > 0){
		    sv->worker_threads = (pthread_t **)Malloc(nr_threads * sizeof(pthread_t *));
		    for (int i = 0; i < nr_threads; i++){
		        sv->worker_threads[i] = (pthread_t *)Malloc(sizeof(pthread_t));
            	err = pthread_create(sv->worker_threads[i], NULL, (void *)&request_stub, (void *)sv);
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
        if (sv->exiting){
            break;
        }

        pthread_mutex_lock(sv->lock);

        while(sv->buff_in == sv->buff_out){
            pthread_cond_wait(sv->empty, sv->lock);
        }

        connfd = sv->request_buff[sv->buff_out];
        sv->request_buff[sv->buff_out] = 0;

        sv->buff_out = (sv->buff_out + 1) % sv->max_requests;

        pthread_cond_signal(sv->full);
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
        if ((sv->buff_in - sv->buff_out + sv->max_requests) % sv->max_requests == sv->max_requests - 1){
            pthread_cond_wait(sv->full, sv->lock);
        }

        sv->request_buff[sv->buff_in] = connfd;

        sv->buff_in = (sv->buff_in + 1) % sv->max_requests;

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
	for (int i = 0; i < sv->nr_threads; i++){
        free(sv->worker_threads[i]);
    }
    free(sv->request_buff);
    free(sv->worker_threads);
    free(sv->empty);
    free(sv->full);
    free(sv->lock);
	/* make sure to free any allocated resources */
	free(sv);
}
