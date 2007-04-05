/* list.h
 *
 * Maintains very lightweight linked lists.
 *
 * Copyright (C) 2006 Scott Bronson
 * This file is released under the MIT license.
 * Please see the COPYING file for specifics.
 */

#ifndef LIST_H
#define LIST_H

#include <assert.h>


struct list_head {
    struct list_head *next, *prev;
};
typedef struct list_head list_head;


/** This inititalizes a list node.
 * 
 * List heads are used to keep track of the lists themselves.
 * List nodes are inserted into structures to allow the structure
 * to be added to a list.
 * 
 * These two things must, of course, be backed by the exact
 * same data structure.  Right now I'm going to try to use
 * the convention that to call list_add on a node, you must
 * previously have called list_reset, and therefore it is
 * a list node.
 * 
 * List heads are never NULL, and are created with list_init.
 */

static inline void list_reset(struct list_head *head)
{
    head->next = (void*)0;
    head->prev = (void*)0;
}


/**
 * Initializes a list head.  There's no need to do this if you
 * immediately add a list_head to a list.  You only need to do
 * it when creating the actual head.
 */

static inline void list_init(struct list_head *head)
{
    head->next = head;
    head->prev = head;
}


/**
 * Inserts the entry after the given head.
 */

static inline void list_add(struct list_head *head, struct list_head *entry)
{
	assert(!entry->prev);
	assert(!entry->next);
    
	head->next->prev = entry;
    entry->next = head->next;
    entry->prev = head;
    head->next = entry;
}


/**
 * Inserts the entry before the given head.
 */

static inline void list_insert(struct list_head *head, struct list_head *entry)
{
	assert(!entry->prev);
	assert(!entry->next);
    
	entry->prev = head->prev;
    head->prev->next = entry;
    head->prev = entry;
    entry->next = head;
}


/**
 * Deletes the first entry from the list.
 */

static inline void list_remove(struct list_head *entry)
{
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
    entry->prev = entry->next = (void*)0;
}


/**
 * Returns true if there are no items in the list
 */

static inline int list_is_empty(const struct list_head *head)
{
	assert(head->prev);
	assert(head->next);
	
    return head->next == head;
}


/**
 * Returns the structure that contains this list.
 * 
 * Use it like this:
 * 
 *     struct my_s {
 *        ...
 *        list_head a_list;
 *        ...
 *     };
 * 
 *     void my_callback(list_head *ptr) {
 *         struct my_s *ss = list_entry(ptr, struct my_s, a_list);
 *         do_something(ss);
 *     }
 * 
 * TODO: add type-safety?  See io_resolve_parent();
 */

#define list_entry(ptr, type, member) \
        ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

/**
 * Allows you to loop over each item in a list.
 * 
 * DO NOT add or remove items while looping.  Use list_for_each_safe()
 * if you need to do this.
 * 
 * These are inspired by similar macros in the Linux kernel.
 * 
 * Use it like this:
 * 
 *  my_func() {
 *    list_head *current_position;
 *    list_for_each(current_position, my_list) {
 *       do_something(current_position);
 *    }
 *  }
 */

#define list_for_each(pos, head) \
        for((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

/**
 * Just like list_for_each() but steps backwards rather than forwards.
 */

#define list_for_each_reverse(pos, head) \
        for((pos) = (head)->prev; (pos) != (head); (pos) = (pos)->prev)

/**
 * Just like list_for_each() but works OK if the currently referenced
 * list item is removed from the list.
 * 
 * NOTE: it does NOT work OK if the following list item is removed!
 * If you have to remove a list item, make sure it's the current one.
 */

#define list_for_each_safe(pos, tmp, head) \
	for((pos) = (head)->next, (tmp) = (pos)->next; (pos) != (head); (pos) = (tmp), (tmp) = (pos)->next)

/** Removes the list from "old" and attaches it onto "new".
 * 
 * The new list is inserted after the existing list (so it's inserted
 * before the dst list head).
 * The old head is left empty.
 * 
 * TODO: this should probably not be inlined.
 */

static inline void list_move(struct list_head *src, struct list_head *dst)
{	
	struct list_head *srclast = src->prev;
	struct list_head *dstlast = dst->prev;
	
	if(!list_is_empty(src)) {
		srclast->next = dst;
		dst->prev = srclast;
		dstlast->next = src->next;
		src->next->prev = dstlast;
		
		list_init(src);
	}
}

#endif
