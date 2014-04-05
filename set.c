#include "set.h"

void set_init(set_t *s, int (*cmp)(const void *, const void *)) {
	s->count = 0;
	s->root = NULL;
	s->cmp = cmp;	
}

void _set_destroy_rec(struct _set_node *node) {
	if (node == NULL) return;
	_set_destroy_rec(node->left);
	_set_destroy_rec(node->right);
	free(node);
}

void set_destroy(set_t *s) {
	_set_destroy_rec(s->root);
	s->root = NULL;
	s->count = 0;
}

const void *set_find(set_t *s, const void *key) {
	int i;
	struct _set_node *node;
	for (node = s->root; node != NULL; ) {
		i = s->cmp(key, node->key);
		if (i < 0) node = node->left;
		else if (i > 0) node = node->right;
		else return node->key;
	}
	return NULL;
}

const void *_set_insert_rec(set_t *s, struct _set_node *parent, struct _set_node *node, const void *key, size_t key_size) {
	int i;
	struct _set_node *new_node;
	i = s->cmp(key, node->key);
	if (i < 0) {
		if (node->left == NULL) {
			new_node = (struct _set_node*)malloc(sizeof(struct _set_node) + key_size);
			node->left = new_node;
			new_node->left = NULL;
			new_node->right = NULL;	
			return memcpy(new_node->key, key, key_size);
		} else return _set_insert_rec(s, node, node->left, key, key_size);
	} else if (i > 0) {
		if (node->right == NULL) {
			new_node = (struct _set_node*)malloc(sizeof(struct _set_node) + key_size);
			node->right = new_node;
			new_node->left = NULL;
			new_node->right = NULL;
			return memcpy(new_node->key, key, key_size);
		} else return _set_insert_rec(s, node, node->right, key, key_size);
	} else {	
		new_node = (struct _set_node*)realloc(node, sizeof(struct _set_node) + key_size);
		if (parent) {
			if (parent->left == node) parent->left = new_node;
			else parent->right = new_node;
		}
		return memcpy(new_node->key, key, key_size);
	}	
}

const void *set_insert(set_t *s, const void *key, size_t key_size) {
	if (s->root == NULL) {
		s->root = (struct _set_node*)malloc(sizeof(struct _set_node) + key_size);
		s->root->left = s->root->right = NULL;
		return memcpy(s->root->key, key, key_size);
	} else return _set_insert_rec(s, NULL, s->root, key, key_size);
}

void _set_replace_node_in_parent(
	struct _set_node *parent,
	struct _set_node *oldnode,
	struct _set_node *newnode
) {
	if (parent != NULL) {
		if (parent->left == oldnode)
			parent->left = newnode;
		else
			parent->right = newnode;
	}
	free(oldnode);
}

int _set_remove_rec(set_t *s, struct _set_node *parent, struct _set_node *node, const void *key) {
	int i;
	struct _set_node *succ;
	i = s->cmp(key, node->key);
	if (i < 0) {
		if (node->left == NULL) return -1;
		return _set_remove_rec(s, node, node->left, key);
	} else if (i > 0) {
		if (node->right == NULL) return -1;
		return _set_remove_rec(s, node, node->right, key);
	} else {
		if (node->left && node->right) {
			parent = node;
			succ = node->right;
			while (succ->left) { parent = succ; succ = succ->left; }
			_set_remove_rec(s, parent, succ, key);
		} else if (node->left) {
			_set_replace_node_in_parent(parent, node, node->left);
		} else if (node->right) {
			_set_replace_node_in_parent(parent, node, node->right);
		} else {
			_set_replace_node_in_parent(parent, node, NULL);
		}
	}
	return 0;
}

int set_remove(set_t *s, const void *key) {
	if (s->root == NULL) return -1;
	return _set_remove_rec(s, NULL, s->root, key);
}

int _set_iterate_rec(struct _set_node *node, int (*callback)(const void *)) {
	int i;
	if (node == NULL) return 0;
	i = _set_iterate_rec(node->left, callback);
	if (i < 0) return i;
	i = callback(node->key);
	if (i < 0) return i;
	i = _set_iterate_rec(node->right, callback);
	return i;
}

int set_iterate(set_t *m, int (*callback)(const void *key)) {
	return _set_iterate_rec(m->root, callback);
}

int string_cmp(const void *a, const void *b) {
	return strcmp((const char *)a, (const char *)b);
}

int int_cmp(const void *a, const void *b) {
	return *(const int*)a - *(const int*)b;
	return *(const int*)a < *(const int*)b;
}

int float_cmp(const void *a, const void *b) {
	return *(const float*)a - *(const float*)b;
}
