#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "hashtable.h"
#include "wc.h"

struct wc {
	char *word_array;
	long int size;
};

struct wc *
wc_init(char *word_array, long size)
{
	struct wc *wc;

	wc = (struct wc *)malloc(sizeof(struct wc));
	assert(wc);

    wc->word_array = strdup(word_array);
    wc->size = size;
	return wc;
}

void
wc_output(struct wc *wc)
{
    HashTable table;
    ht_setup(&table, sizeof(int), sizeof(char*), 10);
    char * word_array_copy = strdup(wc->word_array);

    char * token;
    token = strtok(word_array_copy, " ");
    int i = 0;
    int new_val;
    while (token != NULL)
    {
        new_val = 1;
        if (ht_contains(&table, &token))
            new_val += *(int*)ht_lookup(&table, &token);

        ht_insert(&table, &token, &new_val);
        i++;
        token = strtok(NULL, " ");
    }

    for (int i = 0; i < table.size; i++){
        char * keyword = *(char**)(table.nodes[i]->key);
        printf("%s: %d", keyword, *(int*)ht_lookup(&table, &keyword));
    }

	ht_clear(&table);
    ht_destroy(&table);
}

void
wc_destroy(struct wc *wc)
{
	//TBD();
	free(wc);
}
