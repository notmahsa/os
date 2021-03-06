#include "request.h"
#include "server_thread.h"
#include "common.h"
#include <pthread.h>

struct request {
	int fd;		 /* descriptor for client connection */
	struct file_data *data;
};

struct cache_entry {
    struct file_data * cache_data;
    int in_use;
    
    // Using linked list chaining for collision resolution.
    struct cache_entry * next_in_ll;
    int deleted;
};

struct cache_table {
    struct cache_entry ** entries;
    long table_size;
};

struct rlu_table {
	char * file;
	
	// Using doubly linked list for keeping track of RLU and MRU.
	struct rlu_table * prev;
	struct rlu_table * next;
};

struct server {
	int nr_threads;
	int max_requests;
	int max_cache_size;
	int exiting;
	/* add any other parameters you need */

	pthread_t * worker_threads;
	int * request_buff;
	pthread_mutex_t lock;
    pthread_cond_t full;
    pthread_cond_t empty;
    int buff_in;
    int buff_out;

    struct cache_table * cache;
    struct rlu_table * rlu_table;
    pthread_mutex_t cache_lock;
    int cache_size_used;
};

struct cache_table * cache_init(long size);
struct cache_entry * cache_lookup(struct server *sv, char *file);
struct cache_entry * cache_insert(struct server *sv, const struct request *rq);
int cache_evict(struct server *sv, int bytes_to_evict);
void rlu_update(struct server *sv, const struct request *rq);

int
hash(char *str, long table_size)
{
    int hash = 5381;
    int c = 0;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    if(hash < 0) hash *= -1;
    hash = hash % table_size;
    return hash;
}

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
	if (sv->max_cache_size == 0){
	    ret = request_readfile(rq);
        if (ret != 0){
            request_sendfile(rq);
        }
        request_destroy(rq);
        file_data_free(data);
        return;
	}

    pthread_mutex_lock(&sv->cache_lock);
    struct cache_entry * current = cache_lookup(sv, rq->data->file_name);
    if (current != NULL){
        assert(!strcmp(rq->data->file_name, current->cache_data->file_name));
        rq->data->file_size = current->cache_data->file_size;
        rq->data->file_buf = strdup(current->cache_data->file_buf);
        current->in_use = 1;
        rlu_update(sv, rq);
    }
    else {
        pthread_mutex_unlock(&sv->cache_lock);
        ret = request_readfile(rq);
        if (!ret) goto out;
        pthread_mutex_lock(&sv->cache_lock);
        current = cache_lookup(sv, rq->data->file_name);

        if (current == NULL) {
            current = cache_insert(sv, rq);
            if (current != NULL){
                assert(!strcmp(rq->data->file_name, current->cache_data->file_name));
                current->in_use = 1;
                rlu_update(sv, rq);
            }
        }
        else {
            assert(!strcmp(rq->data->file_name, current->cache_data->file_name));
            rq->data->file_buf = strdup(current->cache_data->file_buf);
            rq->data->file_size = current->cache_data->file_size;
            current->in_use = 1;
            rlu_update(sv, rq);
        }
    }

    pthread_mutex_unlock(&sv->cache_lock);
    request_sendfile(rq);

out:
    if (current != NULL){
        pthread_mutex_lock(&sv->cache_lock);
        current->in_use = 0;
        pthread_mutex_unlock(&sv->cache_lock);
    }

    request_destroy(rq);
    file_data_free(data);
}

void
request_stub(void * sv_void) {
    struct server * sv = (struct server *)sv_void;
    while (sv->exiting == 0){
        pthread_mutex_lock(&sv->lock);

        while (sv->buff_in == sv->buff_out){
            pthread_cond_wait(&sv->empty, &sv->lock);
            if (sv->exiting == 1){
                pthread_mutex_unlock(&sv->lock);
                return;
            }
        }

        int connfd = sv->request_buff[sv->buff_out];
        sv->buff_out = (sv->buff_out + 1) % sv->max_requests;
        pthread_cond_broadcast(&sv->full);
        pthread_mutex_unlock(&sv->lock);
        if (sv->exiting == 1) return;
        do_server_request(sv, connfd);
    }
    pthread_mutex_unlock(&sv->lock);
}

/* entry point functions */

struct server *
server_init(int nr_threads, int max_requests, int max_cache_size)
{
    struct server *sv;

    sv = Malloc(sizeof(struct server));
    sv->nr_threads = nr_threads;
    sv->max_requests = max_requests + 1;
    sv->max_cache_size = max_cache_size;
    sv->exiting = 0;

    int err;
    err = pthread_mutex_init(&sv->lock, NULL);
    assert(err == 0);

    err = pthread_mutex_init(&sv->cache_lock, NULL);
    assert(err == 0);

    err = pthread_cond_init(&sv->full, NULL);
    assert(err == 0);

    err = pthread_cond_init(&sv->empty, NULL);
    assert(err == 0);

    sv->buff_in = 0;
    sv->buff_out = 0;
    sv->cache = NULL;
    sv->rlu_table = NULL;

    if (nr_threads > 0 || max_requests > 0 || max_cache_size > 0){
        if (nr_threads > 0){
            sv->worker_threads = (pthread_t *)malloc(nr_threads * sizeof(pthread_t));

            for (int i = 0; i < nr_threads; i++){
                pthread_create(&sv->worker_threads[i], NULL, (void *)&request_stub, (void *)sv);
            }
        }
        if (max_requests > 0){
            sv->request_buff = (int *)malloc(sv->max_requests * sizeof(int));
        }
        if (max_cache_size > 0){
            sv->cache = (struct cache_table *)malloc(sizeof(struct cache_table));
            sv->cache->table_size = max_cache_size;
            sv->cache->entries = (struct cache_entry **)malloc(max_cache_size * sizeof(struct cache_entry *));
            for (int i = 0; i < sv->cache->table_size; i++) sv->cache->entries[i] = NULL;
        }
    }

    /* Lab 4: create queue of max_request size when max_requests > 0 */

    /* Lab 5: init server cache and limit its size to max_cache_size */

    /* Lab 4: create worker threads when nr_threads > 0 */

    return sv;
}

void
server_request(struct server *sv, int connfd)
{
    if (sv->nr_threads == 0) {
        /* no worker threads */
        do_server_request(sv, connfd);
    } else {
        /*  Save the relevant info in a buffer and have one of the
        *  worker threads do the work. */
        pthread_mutex_lock(&sv->lock);
        if (sv->exiting == 1) {
            pthread_mutex_unlock(&sv->lock);
            return;
        }

        while((sv->buff_in - sv->buff_out + sv->max_requests) % sv->max_requests == sv->max_requests - 1){
            pthread_cond_wait(&sv->full, &sv->lock);
        }

        sv->request_buff[sv->buff_in] = connfd;
        sv->buff_in = (sv->buff_in + 1) % sv->max_requests;
        pthread_cond_broadcast(&sv->empty);
        pthread_mutex_unlock(&sv->lock);
    }
}

struct cache_entry * 
cache_lookup(struct server *sv, char* file)
{
	int hash_index = hash(file, sv->cache->table_size);
    if (sv->cache->entries[hash_index]==NULL) return NULL;
    else {
    	struct cache_entry * current = sv->cache->entries[hash_index];
    	while (current != NULL) {
            if (!current->deleted && !strcmp(current->cache_data->file_name, file)) return current;
            else current = current->next_in_ll;
        }
        return NULL;
    }
}

struct cache_entry *
cache_insert(struct server *sv, const struct request *rq)
{
	if (rq->data->file_size > sv->max_cache_size) return NULL;
	if (sv->cache_size_used + rq->data->file_size > sv->max_cache_size){
	    int bytes_to_evict = sv->cache_size_used + rq->data->file_size - sv->max_cache_size;
	    int evicted = cache_evict(sv, bytes_to_evict);
		if (evicted == 0) return NULL;
	}

	int hash_index = hash(rq->data->file_name, sv->cache->table_size);
    struct cache_entry * new_entry = (struct cache_entry *)malloc(sizeof(struct cache_entry));
    assert(new_entry);

    new_entry->cache_data = file_data_init();
    new_entry->cache_data->file_name = strdup(rq->data->file_name);
    new_entry->cache_data->file_buf = strdup(rq->data->file_buf);
    new_entry->cache_data->file_size = rq->data->file_size;
    new_entry->in_use = 0;
    new_entry->deleted = 0;
	new_entry->next_in_ll = NULL;

	sv->cache_size_used += new_entry->cache_data->file_size;

	if (sv->cache->entries[hash_index] == NULL){
		sv->cache->entries[hash_index] = new_entry;
		return new_entry;
	}
    else {
        struct cache_entry * current = sv->cache->entries[hash_index];
        struct cache_entry * previous = NULL;
        while (current != NULL) {
            if (current->deleted) {
                new_entry->next_in_ll = current->next_in_ll;
                if (previous == NULL) sv->cache->entries[hash_index] = new_entry;
                else previous->next_in_ll = new_entry;
                free(current);
                return new_entry;
            }
            else {
                previous = current;
                current = current->next_in_ll;
            }
        }
        previous->next_in_ll = new_entry;
        return new_entry;
    }
}

int
cache_evict(struct server *sv, int bytes_to_evict){
    int at_capacity = 0;
    struct rlu_table * last = sv->rlu_table;
    if (last == NULL) return 0;

    while(last->next != NULL){
        last = last->next;
    }

    while (bytes_to_evict > 0 && !at_capacity) {
        struct cache_entry * current = cache_lookup(sv, last->file);
        while (!at_capacity && current->in_use != 0){
            last = last->prev;
            if (last != NULL) current = cache_lookup(sv, last->file);
            else at_capacity = 1;
        }
        // Variable "last" here marks the last file in the LRU table, that is not in use.
        if (!at_capacity) {
            if (last->prev != NULL){
                last->prev->next = last->next;
                if (last->next != NULL) last->next->prev = last->prev;
            }
            else {
                sv->rlu_table = last->next;
                at_capacity = 1;
                if (last->next != NULL) last->next->prev = NULL;
            }

            struct rlu_table * temp = last->prev;
            free(last);
            last = temp;
            current->deleted = 1;
            bytes_to_evict -= current->cache_data->file_size;
            sv->cache_size_used = sv->cache_size_used - current->cache_data->file_size;
            file_data_free(current->cache_data);
        }
    }
	if (at_capacity) return 0;
    return bytes_to_evict;
}

void rlu_update(struct server *sv, const struct request *rq)
{
    struct rlu_table * current = sv->rlu_table;
    while (current){
        if (!strcmp(current->file, rq->data->file_name)){
            if (current->prev){
                current->prev->next = current->next;
                if (current->next) current->next->prev = current->prev;
                current->next = sv->rlu_table;
                sv->rlu_table->prev = current;
                sv->rlu_table = current;
                current->prev = NULL;
            }
            return;
        }
        current = current->next;
    }

	if (!sv->rlu_table){
		sv->rlu_table = (struct rlu_table*)malloc(sizeof(struct rlu_table));
		assert(sv->rlu_table);
		sv->rlu_table->file = strdup(rq->data->file_name);
		sv->rlu_table->next = NULL;
		sv->rlu_table->prev = NULL;
		return;
	}

    struct rlu_table * new_rlu_entry = (struct rlu_table *)malloc(sizeof(struct rlu_table));
    new_rlu_entry->file = strdup(rq->data->file_name);
    new_rlu_entry->next = sv->rlu_table;
    new_rlu_entry->prev = NULL;

    sv->rlu_table->prev = new_rlu_entry;
    sv->rlu_table = new_rlu_entry;
}

void server_exit(struct server *sv)
{
    /* when using one or more worker threads, use sv->exiting to indicate to
    * these threads that the server is exiting. make sure to call
    * pthread_join in this function so that the main server thread waits
    * for all the worker threads to exit before exiting. */

    pthread_mutex_lock(&sv->lock);
    sv->exiting = 1;

    pthread_cond_broadcast(&sv->empty);
    pthread_mutex_unlock(&sv->lock);

    for (int i = 0; i < sv->nr_threads; i++){
        pthread_join(sv->worker_threads[i], NULL);
    }

    free(sv->request_buff);
    free(sv->worker_threads);
    pthread_mutex_destroy(&sv->lock);
    pthread_cond_destroy(&sv->empty);
    pthread_cond_destroy(&sv->full);

    /* make sure to free any allocated resources */
    free(sv);
    return;
}
