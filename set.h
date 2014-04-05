//Set data structure implemented as a binary search tree

#ifndef C_SET_H 
#define C_SET_H 

#include <stdlib.h>
#include <string.h>

int string_cmp(const void *a, const void *b);
int int_cmp(const void *a, const void *b);
int float_cmp(const void *a, const void *b);

struct _set_node {
	struct _set_node *left, *right;
	char key[0];
};

typedef struct {
	size_t count;
	struct _set_node *root;
	int (*cmp)(const void *a, const void *b);
} set_t;

#define set_count(M) (M)->count
	
void		set_init(set_t *m, int (*cmp)(const void *, const void *));
void		set_destroy(set_t *m);
const void	*set_insert(set_t *m, const void *key, size_t key_size);
int		set_remove(set_t *m, const void *key);
const void	*set_find(set_t *m, const void *key);
void		set_clear(set_t *m);
int		set_iterate(set_t *m, int (*callback)(const void *key));

#endif
