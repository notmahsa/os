#ifndef _WC_H_
#define _WC_H_

/* DO NOT CHANGE THIS FILE */

/* Forward declaration of structure for the function declarations below. */
struct wc;

/* You will be writing these functions */

/* Initialize wc data structure, returning pointer to it. The input to this
 * function is an array of characters. The length of this array is size.  The
 * array contains a sequence of words, separated by spaces. You need to parse
 * this array for words, and initialize your data structure with the words in
 * the array. You can use the isspace() function to look for spaces between
 * words. You can use strcmp() to compare for equality of two words. Note that
 * the array is read only and cannot be modified. */
struct wc *wc_init(char *word_array, long size);

/* wc_output produces output, consisting of unique words that have been inserted
 * in wc (in wc_init), and a count of the number of times each word has been
 * seen.
 *
 * The output should be sent to standard output, i.e., using the standard printf
 * function.
 *
 * The output should be in the format shown below. The words do not have to be
  * sorted in any order. Do not add any extra whitespace.
word1:5
word2:10
word3:30
word4:1
 */
void wc_output(struct wc *wc);

/* Destroy all the data structures you have created, so you have no memory
 * loss. */
void wc_destroy(struct wc *wc);






#include <stdbool.h>
#include <stddef.h>

/****************** DEFINTIIONS ******************/

#define HT_MINIMUM_CAPACITY 8
#define HT_LOAD_FACTOR 5
#define HT_MINIMUM_THRESHOLD (HT_MINIMUM_CAPACITY) * (HT_LOAD_FACTOR)

#define HT_GROWTH_FACTOR 2
#define HT_SHRINK_THRESHOLD (1 / 4)

#define HT_ERROR -1
#define HT_SUCCESS 0

#define HT_UPDATED 1
#define HT_INSERTED 0

#define HT_NOT_FOUND 0
#define HT_FOUND 01

typedef int (*comparison_t)(void*, void*, size_t);
typedef size_t (*hash_t)(void*, size_t);

/****************** STRUCTURES ******************/

typedef struct HTNode {
	struct HTNode* next;
	void* key;
	void* value;

} HTNode;

typedef struct HashTable {
	size_t size;
	size_t threshold;
	size_t capacity;

	size_t key_size;
	size_t value_size;

	comparison_t compare;
	hash_t hash;

	HTNode** nodes;

} HashTable;

/****************** INTERFACE ******************/

/* Setup */
int ht_setup(HashTable* table,
						 size_t key_size,
						 size_t value_size,
						 size_t capacity);

int ht_copy(HashTable* first, HashTable* second);
int ht_move(HashTable* first, HashTable* second);
int ht_swap(HashTable* first, HashTable* second);

/* Destructor */
int ht_destroy(HashTable* table);

int ht_insert(HashTable* table, void* key, void* value);

int ht_contains(HashTable* table, void* key);
void* ht_lookup(HashTable* table, void* key);
const void* ht_const_lookup(const HashTable* table, void* key);

int ht_erase(HashTable* table, void* key);
int ht_clear(HashTable* table);

int ht_is_empty(HashTable* table);
bool ht_is_initialized(HashTable* table);

int ht_reserve(HashTable* table, size_t minimum_capacity);

/****************** PRIVATE ******************/

void _ht_int_swap(size_t* first, size_t* second);
void _ht_pointer_swap(void** first, void** second);

size_t _ht_default_hash(void* key, size_t key_size);
size_t _ht_string_hash(void* key, size_t key_size);
int _ht_default_compare(void* first_key, void* second_key, size_t key_size);
int _ht_string_compare(void* first_key, void* second_key, size_t key_size);

size_t _ht_hash(const HashTable* table, void* key);
bool _ht_equal(const HashTable* table, void* first_key, void* second_key);

bool _ht_should_grow(HashTable* table);
bool _ht_should_shrink(HashTable* table);

HTNode* _ht_create_node(HashTable* table, void* key, void* value, HTNode* next);
int _ht_push_front(HashTable* table, size_t index, void* key, void* value);
void _ht_destroy_node(HTNode* node);

int _ht_adjust_capacity(HashTable* table);
int _ht_allocate(HashTable* table, size_t capacity);
int _ht_resize(HashTable* table, size_t new_capacity);
void _ht_rehash(HashTable* table, HTNode** old, size_t old_capacity);





#endif /* _WC_H_ */
