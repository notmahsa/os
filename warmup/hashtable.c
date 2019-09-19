#include <assert.h>
#include <stdlib.h>
#include <string.h>


#include <stdio.h>


#include "hashtable.h"

int ht_setup(HashTable* table,
						 size_t key_size,
						 size_t value_size,
						 size_t capacity) {
	assert(table != NULL);

	if (table == NULL) return HT_ERROR;

	if (capacity < HT_MINIMUM_CAPACITY) {
		capacity = HT_MINIMUM_CAPACITY;
	}

	if (_ht_allocate(table, capacity) == HT_ERROR) {
		return HT_ERROR;
	}

	table->key_size = key_size;
	table->value_size = value_size;
	table->hash = _ht_string_hash;
	table->compare = _ht_string_compare;
	table->size = 0;

	return HT_SUCCESS;
}

int ht_destroy(HashTable* table) {
	HTNode* node;
	HTNode* next;
	size_t chain;

	assert(ht_is_initialized(table));
	if (!ht_is_initialized(table)) return HT_ERROR;

	for (chain = 0; chain < table->capacity; ++chain) {
		node = table->nodes[chain];
		while (node) {
			next = node->next;
			_ht_destroy_node(node);
			node = next;
		}
	}

	free(table->nodes);

	return HT_SUCCESS;
}

int ht_insert(HashTable* table, void* key, void* value) {
	size_t index;
	HTNode* node;

	assert(ht_is_initialized(table));
	assert(key != NULL);

	if (!ht_is_initialized(table)) return HT_ERROR;
	if (key == NULL) return HT_ERROR;

	if (_ht_should_grow(table)) {
		_ht_adjust_capacity(table);
	}

	index = _ht_hash(table, key);
	for (node = table->nodes[index]; node; node = node->next) {
		if (_ht_equal(table, key, node->key)) {
			memcpy(node->value, value, table->value_size);
			return HT_UPDATED;
		}
	}

	if (_ht_push_front(table, index, key, value) == HT_ERROR) {
		return HT_ERROR;
	}

	++table->size;

	return HT_INSERTED;
}

int ht_contains(HashTable* table, void* key) {
	size_t index;
	HTNode* node;

	assert(ht_is_initialized(table));
	assert(key != NULL);

	if (!ht_is_initialized(table)) return HT_ERROR;
	if (key == NULL) return HT_ERROR;

	index = _ht_hash(table, key);
	for (node = table->nodes[index]; node; node = node->next) {
		if (_ht_equal(table, key, node->key)) {
			return 1;
		}
	}

	return 0;
}

void* ht_lookup(HashTable* table, void* key) {
	HTNode* node;
	size_t index;

	assert(table != NULL);
	assert(key != NULL);

	if (table == NULL) return NULL;
	if (key == NULL) return NULL;

	index = _ht_hash(table, key);
	for (node = table->nodes[index]; node; node = node->next) {
		if (_ht_equal(table, key, node->key)) {
			return node->value;
		}
	}

	return NULL;
}

int ht_erase(HashTable* table, void* key) {
	HTNode* node;
	HTNode* previous;
	size_t index;

	assert(table != NULL);
	assert(key != NULL);

	if (table == NULL) return HT_ERROR;
	if (key == NULL) return HT_ERROR;

	index = _ht_hash(table, key);
	node = table->nodes[index];

	for (previous = NULL; node; previous = node, node = node->next) {
		if (_ht_equal(table, key, node->key)) {
			if (previous) {
				previous->next = node->next;
			} else {
				table->nodes[index] = node->next;
			}

			_ht_destroy_node(node);
			--table->size;

			if (_ht_should_shrink(table)) {
				if (_ht_adjust_capacity(table) == HT_ERROR) {
					return HT_ERROR;
				}
			}

			return HT_SUCCESS;
		}
	}

	return HT_NOT_FOUND;
}

int ht_clear(HashTable* table) {
	assert(table != NULL);
	assert(table->nodes != NULL);

	if (table == NULL) return HT_ERROR;
	if (table->nodes == NULL) return HT_ERROR;

	ht_destroy(table);
	_ht_allocate(table, HT_MINIMUM_CAPACITY);
	table->size = 0;

	return HT_SUCCESS;
}

bool ht_is_initialized(HashTable* table) {
	return table != NULL && table->nodes != NULL;
}

/****************** PRIVATE ******************/

int _ht_string_compare(void* first_key, void* second_key, size_t key_size) {
    char * first_key_char;
    char * second_key_char;
    first_key_char = (char*)first_key;
    second_key_char = (char*)second_key;
    // printf("1: %s, 2: %s - %d\n", first_key_char, second_key_char, strcmp(first_key, second_key));
	return strcmp(first_key_char, second_key_char);
}

size_t _ht_string_hash(void* raw_key, size_t key_size){
    size_t hash = 5381;
    char *str = (char*) raw_key;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

size_t _ht_hash(const HashTable* table, void* key) {
#ifdef HT_USING_POWER_OF_TWO
	return table->hash(key, table->key_size) & table->capacity;
#else
	return table->hash(key, table->key_size) % table->capacity;
#endif
}

bool _ht_equal(const HashTable* table, void* first_key, void* second_key) {
	return table->compare(first_key, second_key, table->key_size) == 0;
}

bool _ht_should_grow(HashTable* table) {
	assert(table->size <= table->capacity);
	return table->size == table->capacity;
}

bool _ht_should_shrink(HashTable* table) {
	assert(table->size <= table->capacity);
	return table->size == table->capacity * HT_SHRINK_THRESHOLD;
}

HTNode*
_ht_create_node(HashTable* table, void* key, void* value, HTNode* next) {
	HTNode* node;

	assert(table != NULL);
	assert(key != NULL);
	assert(value != NULL);

	if ((node = malloc(sizeof *node)) == NULL) {
		return NULL;
	}
	if ((node->key = malloc(table->key_size)) == NULL) {
		return NULL;
	}
	if ((node->value = malloc(table->value_size)) == NULL) {
		return NULL;
	}

	memcpy(node->key, key, table->key_size);
	memcpy(node->value, value, table->value_size);
	node->next = next;

	return node;
}

int _ht_push_front(HashTable* table, size_t index, void* key, void* value) {
	table->nodes[index] = _ht_create_node(table, key, value, table->nodes[index]);
	return table->nodes[index] == NULL ? HT_ERROR : HT_SUCCESS;
}

void _ht_destroy_node(HTNode* node) {
	assert(node != NULL);

	free(node->key);
	free(node->value);
	free(node);
}

int _ht_adjust_capacity(HashTable* table) {
	return _ht_resize(table, table->size * HT_GROWTH_FACTOR);
}

int _ht_allocate(HashTable* table, size_t capacity) {
	if ((table->nodes = malloc(capacity * sizeof(HTNode*))) == NULL) {
		return HT_ERROR;
	}
	memset(table->nodes, 0, capacity * sizeof(HTNode*));

	table->capacity = capacity;
	table->threshold = capacity * HT_LOAD_FACTOR;

	return HT_SUCCESS;
}

int _ht_resize(HashTable* table, size_t new_capacity) {
	HTNode** old;
	size_t old_capacity;

	if (new_capacity < HT_MINIMUM_CAPACITY) {
		if (table->capacity > HT_MINIMUM_CAPACITY) {
			new_capacity = HT_MINIMUM_CAPACITY;
		} else {
			/* NO-OP */
			return HT_SUCCESS;
		}
	}

	old = table->nodes;
	old_capacity = table->capacity;
	if (_ht_allocate(table, new_capacity) == HT_ERROR) {
		return HT_ERROR;
	}

	_ht_rehash(table, old, old_capacity);

	free(old);

	return HT_SUCCESS;
}

void _ht_rehash(HashTable* table, HTNode** old, size_t old_capacity) {
	HTNode* node;
	HTNode* next;
	size_t new_index;
	size_t chain;

	for (chain = 0; chain < old_capacity; ++chain) {
		for (node = old[chain]; node;) {
			next = node->next;

			new_index = _ht_hash(table, node->key);
			node->next = table->nodes[new_index];
			table->nodes[new_index] = node;

			node = next;
		}
	}
}