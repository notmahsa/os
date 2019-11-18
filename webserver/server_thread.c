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
    pthread_mutex_t cache_lock;
    int cache_size_used;
};

struct cache_table * cache_init(long size);
struct cache_entry * cache_lookup(struct server *sv, char *file);
struct cache_entry * cache_insert(struct server *sv, const struct request *rq);
int cache_evict(struct server *sv, int bytes_to_evict);
void exist_list_updater(const struct request *rq);
void new_list_updater(const struct request *rq);
struct rlu_table * rlu_table = NULL;

int
hash(char *str, long table_size)
{
    int hash = 5381;
    int c = 0;
    while ((c = *str++)){
        hash = ((hash << 5) + hash) + c;
    }
    if(hash < 0){
        hash *= -1;
    }
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
    struct cache_entry * current_element = cache_lookup(sv, rq->data->file_name);
    if (current_element != NULL){
        assert(!strcmp(rq->data->file_name, current_element->cache_data->file_name));
        rq->data->file_buf = current_element->cache_data->file_buf;
        rq->data->file_size = current_element->cache_data->file_size;
        current_element->in_use++;
        exist_list_updater(rq);
    }
    else {
        pthread_mutex_unlock(&sv->cache_lock);
        ret = request_readfile(rq);
        if (!ret) goto out;
        pthread_mutex_lock(&sv->cache_lock);
        current_element = cache_lookup(sv, rq->data->file_name);
        if (current_element == NULL) {
            current_element = cache_insert(sv, rq);
            if(current_element != NULL){
                assert(!strcmp(rq->data->file_name, current_element->cache_data->file_name));
                current_element->in_use++;
                new_list_updater(rq);
            }
        }
        else {
            assert(!strcmp(rq->data->file_name, current_element->cache_data->file_name));
            rq->data->file_buf = current_element->cache_data->file_buf
            ;
            rq->data->file_size = current_element->cache_data->file_size;
            current_element->in_use++;
            exist_list_updater(rq);
        }
    }

    pthread_mutex_unlock(&sv->cache_lock);
    request_sendfile(rq);
out:
    if (current_element != NULL){
        pthread_mutex_lock(&sv->cache_lock);
        current_element->in_use--;
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
            sv->cache->entries = (struct cache_entry **)malloc(sizeof(struct cache_entry *) * max_cache_size);
            for(int i = 0; i < sv->cache->table_size; i++) sv->cache->entries[i] = NULL;
        }
        else {
            sv->cache = NULL;
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

struct cache_entry * cache_lookup(struct server *sv, char* file)
{
	int hash_value = hash(file, sv->cache->table_size);
    if(sv->cache->entries[hash_value]==NULL)
       	return NULL;
    else {
    	struct cache_entry * current_element = sv->cache->entries[hash_value];
    	while (current_element != NULL) {
            if (!current_element->deleted && !strcmp(current_element->cache_data->file_name, file)) return current_element;
            else current_element = current_element->next_in_ll;
        }
        return NULL;
    }
}

struct cache_entry*
cache_insert(struct server *sv, const struct request *rq)
{
	if (rq->data->file_size > sv->max_cache_size) return NULL;
	if (sv->cache_size_used + rq->data->file_size > sv->max_cache_size){
	    int bytes_to_evict = sv->cache_size_used + rq->data->file_size - sv->max_cache_size;
		int temp = cache_evict(sv, bytes_to_evict);
		if (temp == 0) return NULL;
	}

	sv->cache_size_used += rq->data->file_size;
	int hash_value = hash(rq->data->file_name, sv->cache->table_size);
    struct cache_entry * new_element = (struct cache_entry *)malloc(sizeof(struct cache_entry));
    assert(new_element);

    new_element->cache_data = file_data_init();
    new_element->cache_data->file_name = rq->data->file_name;
    new_element->cache_data->file_buf = rq->data->file_buf;
    new_element->cache_data->file_size = rq->data->file_size;
    new_element->in_use = 0;
    new_element->deleted = 0;
	new_element->next_in_ll = NULL;

	if (sv->cache->entries[hash_value] == NULL){
		sv->cache->entries[hash_value] = new_element;
		return new_element;
	}
    else {
        struct cache_entry * current_element = sv->cache->entries[hash_value];
        struct cache_entry * previous_element = NULL;
        while (current_element != NULL) {
            if (current_element->deleted) {
                new_element->next_in_ll = current_element->next_in_ll;
                if (previous_element == NULL)
                	sv->cache->entries[hash_value] = new_element;
                else
                	previous_element->next_in_ll = new_element;
               free(current_element);
                return new_element;
            }
            else {
                previous_element = current_element;
                current_element = current_element->next_in_ll;
            }
        }
        previous_element->next_in_ll = new_element;
        return new_element;
    }
}

int
cache_evict(struct server *sv, int bytes_to_evict){
    int at_capacity = 0;
	if (rlu_table == NULL)
        assert(0);
    else {
    	struct rlu_table * current_node = rlu_table;
    	struct rlu_table * last_node = NULL;
    	while(current_node->next != NULL){
            current_node = current_node->next;
        }
        last_node = current_node;
    	while (bytes_to_evict > 0 && !at_capacity) {
        	struct cache_entry * current_element = cache_lookup(sv, last_node->file);
        	while (!at_capacity && current_element->in_use != 0){
        		last_node = last_node->prev;
           		if (last_node != NULL) current_element = cache_lookup(sv, last_node->file);
        		else at_capacity = 1;
        	}

        	if (!at_capacity) {
        		bytes_to_evict -= current_element->cache_data->file_size;
        		sv->cache_size_used = sv->cache_size_used - current_element->cache_data->file_size;
        		if (last_node->prev != NULL){
					last_node->prev->next = last_node->next;
					if(last_node->next!=NULL) last_node->next->prev = last_node->prev;
				}
				else {
					rlu_table = last_node->next;
					at_capacity = 1;
					if (last_node->next != NULL) last_node->next->prev = NULL;
				}
				struct rlu_table * temp = last_node;
				last_node = last_node->prev;
        		free(temp);
        		current_element->deleted = 1;
        		file_data_free(current_element->cache_data);
        		current_element->cache_data = NULL;
        	}
    	}
    }
	if (at_capacity) return 0;
    else return bytes_to_evict;
}

void exist_list_updater(const struct request *rq)
{
	struct rlu_table * current_node = rlu_table;
    while (current_node != NULL){
        if (!strcmp(current_node->file, rq->data->file_name)){
            if(current_node->prev != NULL){
            	current_node->prev->next = current_node->next;
            	if(current_node->next != NULL) current_node->next->prev = current_node->prev;
				current_node->next = rlu_table;
				rlu_table->prev = current_node;
				rlu_table = current_node;
				current_node->prev = NULL;
            }
            return;
        }
        current_node = current_node->next;
    }
    assert(0);
}

void new_list_updater(const struct request *rq)
{
	if(!rlu_table) {
		rlu_table = (struct rlu_table*)malloc(sizeof(struct rlu_table));
		assert(rlu_table);
		rlu_table->file = rq->data->file_name;
		rlu_table->next = NULL;
		rlu_table->prev = NULL;
	}
	else
	{
		struct rlu_table * new_node = (struct rlu_table *)malloc(sizeof(rlu_table));
		assert(new_node);
		new_node->file = rq->data->file_name;
		assert(new_node->file);
		rlu_table->prev = new_node;
		new_node->next = rlu_table;
		new_node->prev = NULL;
		rlu_table = new_node;
	}
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

