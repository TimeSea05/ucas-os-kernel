/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * Copyright (C) 2018 Institute of Computing
 * Technology, CAS Author : Han Shukai (email :
 * hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * Changelog: 2019-8 Reimplement queue.h.
 * Provide Linux-style doube-linked list instead of original
 * unextendable Queue implementation. Luming
 * Wang(wangluming@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * */

#ifndef INCLUDE_LIST_H_
#define INCLUDE_LIST_H_

#include <type.h>

// double-linked list
typedef struct list_node
{
    struct list_node *next, *prev;
} list_node_t;

typedef list_node_t list_head;

// LIST_HEAD is used to define the head of a list.
#define LIST_HEAD(name) struct list_node name = {&(name), &(name)}

#define INIT_LIST_HEAD(head) (head)->prev = (head), (head)->next = (head)

static inline int is_queue_empty(list_node_t *queue) {
  return queue->next == queue; 
}

static inline void list_insert(list_node_t *new, list_node_t *prev, list_node_t *next) {
	next->prev = new;
	prev->next = new;
	new->next = next;
	new->prev = prev;
}

static inline void list_add_tail(list_node_t *new, list_node_t *head) {
	list_insert(new, head->prev, head);
}

static inline void list_delete_entry(list_node_t *entry) {
    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;
}

#define GET_PCB_FROM_LIST(list) (pcb_t *)((ptr_t)list - 2 * sizeof(reg_t) - 2 * sizeof(ptr_t))

/* Offset of member MEMBER in a struct of type TYPE. */
#define offsetof(TYPE, MEMBER) __builtin_offsetof(TYPE, MEMBER)

#define list_entry(ptr, type)  (type *)((char *)ptr - offsetof(type, list))

#define list_for_each_entry(pos, head) \
    for (pos = list_entry((head)->next, typeof(*pos)); \
        	&pos->list != (head); \
        	pos = list_entry(pos->list.next, typeof(*pos)))

#define list_for_each_entry_safe(pos, q, head) \
    for (pos = list_entry((head)->next, typeof(*pos)), \
	        q = list_entry(pos->list.next, typeof(*pos)); \
	        &pos->list != (head); \
	        pos = q, q = list_entry(pos->list.next, typeof(*q)))

#endif
