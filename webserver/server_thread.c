#include "request.h"
#include "server_thread.h"
#include "common.h"
#include <pthread.h>

struct table_entry {
  char * word;
  int in_use;
  int done_caching;
  struct file_data * cache_data;
  struct table_entry * next;
  struct table_entry * more_recently_used;
  struct table_entry * less_recently_used;
};

struct wc {
    struct table_entry ** word_table;
    long table_size;
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

    struct wc * cache;
    struct table_entry * least_recently_used;
    struct table_entry * most_recently_used;
    pthread_mutex_t cache_lock;
    int cache_remaining;
};

struct wc *
wc_init(long size)
{
	struct wc * wc;
	wc = (struct wc *)malloc(sizeof(struct wc));
	assert(wc);
	wc->word_table = (struct table_entry **)malloc(size * sizeof(struct table_entry *));
	assert(wc->word_table);
	wc->table_size = size;
	return wc;
}

static void
file_data_free(struct file_data *data);
int
cache_evict(struct server *sv, int bytes_to_evict);

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

struct table_entry *
cache_lookup(struct server *sv, struct wc *wc, char *str, int index)
{
    if(wc->word_table[index] == NULL){
      printf("%s not found, quitting\n",str);
      return NULL;
    }
    struct table_entry * current_index = wc->word_table[index];
    struct table_entry * tmp;
    while(current_index != NULL){
        if(strcmp(str,current_index->cache_data->file_name)==0){
            if(sv->most_recently_used != current_index){
                current_index->more_recently_used->less_recently_used = current_index->less_recently_used;
                tmp = current_index->more_recently_used;
                current_index->more_recently_used = NULL;
                if(sv->least_recently_used != current_index){
                    current_index->less_recently_used->more_recently_used = tmp;
                }
                else{
                    sv->least_recently_used = tmp;
                }
                sv->most_recently_used->more_recently_used = current_index;
                current_index->less_recently_used = sv->most_recently_used;
                sv->most_recently_used = current_index;
            }
            pthread_mutex_unlock(&sv->cache_lock);
            return current_index;
        }
        current_index = current_index -> next;
    }
    return NULL;
}

struct table_entry *
cache_add(struct server *sv, struct file_data *fd, struct wc *wc)
{
    struct table_entry *lookup_ret = cache_lookup(sv, sv->cache, fd->file_name, hash(fd->file_name,sv->max_cache_size));
    if(lookup_ret != NULL){
        return lookup_ret;
    }
    int index = hash(fd->file_name, wc->table_size); //find the index our data should go to (based on our hash function)
    int evict_ret;
    if(fd->file_size > sv->max_cache_size){
        return NULL;
    }
    if(fd->file_size > sv->cache_remaining){
        printf("attempting to evict files to make room for %s\n",fd->file_name);
        evict_ret = cache_evict(sv,fd->file_size-sv->cache_remaining);
        if(evict_ret == -1){
            return NULL;
        }
    }

    if(wc->word_table[index] == NULL){
        wc->word_table[index] = (struct table_entry *)malloc(sizeof(struct table_entry));
        wc->word_table[index]->cache_data = fd;
        wc->word_table[index]->next = NULL;
        wc->word_table[index]->in_use = 0;

        wc->word_table[index]->more_recently_used = NULL;
        wc->word_table[index]->less_recently_used = sv->most_recently_used;
        if(sv->most_recently_used != NULL){
            sv->most_recently_used->more_recently_used = wc->word_table[index];
        }
        else {
            sv->least_recently_used = wc->word_table[index];
        }
        sv->most_recently_used = wc->word_table[index];
    }
    else {
        struct table_entry *end_of_list = wc->word_table[index];
        while(end_of_list != NULL){
            if(strcmp(end_of_list->cache_data->file_name,fd->file_name) == 0){
                printf("%s already cached, no add is performed\n",end_of_list->cache_data->file_name);
                return end_of_list;
            }
            end_of_list = end_of_list->next;
        }
        struct table_entry *te = (struct table_entry *)malloc(sizeof(struct table_entry));
        te->cache_data = fd;
        te->next = NULL;
        wc->word_table[index]->more_recently_used = NULL;
        wc->word_table[index]->less_recently_used = sv->most_recently_used;
        if(sv->most_recently_used != NULL){
            sv->most_recently_used->more_recently_used = wc->word_table[index];
        }
        else {
            sv->least_recently_used = wc->word_table[index];
        }
        sv->most_recently_used = wc->word_table[index];
        end_of_list->next = te;
    }
    printf("added %s to the cache.\n",fd->file_name);
    sv->cache_remaining = sv->cache_remaining - fd->file_size;
    return wc->word_table[index];
}

int
cache_evict(struct server *sv, int bytes_to_evict)
{
    printf("entered cache_evict\n");
    struct table_entry *in_use_index = sv->least_recently_used;
    int in_use_bytes = 0;
    while(in_use_index != NULL) {
        if(in_use_index->in_use) {
            printf("%s in use -- don't evict me!\n",in_use_index->cache_data->file_name);
            in_use_bytes += in_use_index->cache_data->file_size;
        }
        in_use_index = in_use_index->more_recently_used;
    }
    if(sv->max_cache_size - in_use_bytes < bytes_to_evict) {
        printf("not enough memory, quitting cache_evict\n");
        return -1;
    }

    int bytes_evicted = 0;
    struct table_entry *tmp;
    printf("least recently used block: %s\n",sv->least_recently_used->cache_data->file_name);
    struct table_entry *index = sv->cache->word_table[hash(sv->least_recently_used->cache_data->file_name, sv->max_cache_size)];
    struct table_entry *prev = NULL;
    struct table_entry *eviction_index = sv->least_recently_used;
    while(bytes_evicted < bytes_to_evict && eviction_index != NULL) {
        if(eviction_index->in_use == 0) {
            tmp = eviction_index;
            eviction_index = eviction_index->more_recently_used;
            printf("evicting %s\n",tmp->cache_data->file_name);
            if(sv->cache->word_table[hash(tmp->cache_data->file_name, sv->max_cache_size)]->next == NULL) {
                if(tmp == sv->least_recently_used) {
                    sv->least_recently_used = sv->least_recently_used->more_recently_used;
                    if(sv->least_recently_used == NULL) //evicting only member of cache
                    sv->most_recently_used = NULL;
                }
                else {
                    tmp->less_recently_used->more_recently_used = tmp->more_recently_used;
                    if(tmp != sv->most_recently_used){
                        tmp->more_recently_used->less_recently_used = tmp->less_recently_used;
                    }
                    else {
                        sv->most_recently_used = tmp->less_recently_used;
                    }
                }
                sv->cache_remaining += tmp->cache_data->file_size;
                sv->cache->word_table[hash(tmp->cache_data->file_name,sv->max_cache_size)] = NULL;
                file_data_free(tmp->cache_data);
                free(tmp);
            }
            else {
                while(index != NULL && strcmp(index->cache_data->file_name, tmp->cache_data->file_name)!= 0) {
                    prev = index;
                    index = index->next;
                }
                if(prev == NULL){
                    sv->cache->word_table[hash(tmp->cache_data->file_name,sv->max_cache_size)] = index->next;
                }
                else{
                    prev->next = index->next;
                }
                if(tmp == sv->least_recently_used) {
                    sv->least_recently_used = sv->least_recently_used->more_recently_used;
                    if(sv->least_recently_used == NULL){
                        sv->most_recently_used = NULL;
                    }
                }
                else {
                    tmp->less_recently_used->more_recently_used = tmp->more_recently_used;
                    if(tmp != sv->most_recently_used){
                        tmp->more_recently_used->less_recently_used = tmp->less_recently_used;
                    }
                    else {
                        sv->most_recently_used = tmp->less_recently_used;
                    }
                }

                sv->cache_remaining += tmp->cache_data->file_size;
                file_data_free(tmp->cache_data);
                free(tmp);
            }
        }
        else{
            eviction_index = eviction_index->more_recently_used;
        }
    }
    printf("files successfully evicted\n");
    return 1;
}

void table_entry_delete(struct table_entry *table)
{
    if(table->next != NULL){
        table_entry_delete(table->next);
    }
    free(table->word);
    free(table);
}

void
wc_destroy(struct wc *wc)
{
    int i = 0;
    while(i < wc->table_size) {
        if(wc->word_table[i] != NULL){
            table_entry_delete(wc->word_table[i]);
        }
        i++;
    }
    free(wc->word_table);
    free(wc);
}

void worker_request_loop(void *sv);

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
	if (sv->max_cache_size > 0){
        pthread_mutex_lock(&sv->cache_lock);
        cache_fd = cache_lookup(sv, sv->cache, data->file_name, hash(data->file_name,sv->max_cache_size));
        if (cache_fd == NULL) {
            printf("cache miss, requesting file from disk.\n");
            pthread_mutex_unlock(sv->cache_lock);
            ret = request_readfile(rq);
            pthread_mutex_lock(&sv->cache_lock);
            cache_fd = cache_add(sv,data,sv->cache);
            if(cache_fd != NULL) {
                cache_fd->in_use++;
            }
            pthread_mutex_unlock(sv->cache_lock);
        }
        else {
            printf("cache hit, sending %s.\n",cache_fd->cache_data->file_name);
            data->file_buf = cache_fd->cache_data->file_buf;
            data->file_size = cache_fd->cache_data->file_size;
            cache_fd->in_use++; //to indicate how many files are using the data, as there could be more than one.
            pthread_mutex_unlock(sv->cache_lock);

            request_sendfile(rq);
            pthread_mutex_lock(&sv->cache_lock);
            cache_fd->in_use--; //no longer using the data -- safe to evict.
            printf("%s finished sending, %d other requests sending it now.\n",cache_fd->cache_data->file_name,cache_fd->in_use);
            pthread_mutex_unlock(sv->cache_lock);
            goto out;
        }
        if (!ret) {
          goto out;
        }
        /* sends file to client */
        printf("cache miss, sending %s to client\n", data->file_name);
        request_sendfile(rq);
        if(cache_fd != NULL) {
            pthread_mutex_lock(&sv->cache_lock);
            cache_fd->in_use--;
            printf("%s finished sending, %d other requests sending it now.\n",cache_fd->cache_data->file_name,cache_fd->in_use);
            pthread_mutex_unlock(sv->cache_lock);
        }
    }
    else {
        ret = request_readfile(rq);
        if (ret == 0) { /* couldn't read file */
            goto out;
        }
	}
	/* send file to client */
	request_sendfile(rq);
out:
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

struct server *server_init(int nr_threads, int max_requests, int max_cache_size)
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
            sv->cache = wc_init(max_cache_size);
            sv->cache_remaining = max_cache_size;
        }
    }

    /* Lab 4: create queue of max_request size when max_requests > 0 */

    /* Lab 5: init server cache and limit its size to max_cache_size */

    /* Lab 4: create worker threads when nr_threads > 0 */

    return sv;
}

void server_request(struct server *sv, int connfd)
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
