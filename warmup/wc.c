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
    ht_setup(&table, sizeof(char) * 20, sizeof(int), 10);
    char * word_array_copy;
    word_array_copy = strdup(wc->word_array);

    char * token;
//    char * key;
    token = strtok(word_array_copy, " \n");
    int i;
    i = 0;
    int new_val;
    while (token != NULL)
    {
        new_val = 1;
        if (ht_contains(&table, token)){
            new_val += *(int*)ht_lookup(&table, token);
        }
        ht_insert(&table, token, &new_val);
        i++;
        token = strtok(NULL, " \n");
    }

    HTNode* node;
    size_t index;
    for (index = 0; index < table.capacity; index++) {
        for (node = table.nodes[index]; node; node = node->next) {
            printf("%s:%d\n", (char*)node->key, *(int*)node->value);
        }
    }

	ht_clear(&table);
    ht_destroy(&table);
}

void
wc_destroy(struct wc *wc)
{
	free(wc->word_array);
	free(wc);
}
