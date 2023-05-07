#ifndef USERPROG_PAGEDIR_H
#define USERPROG_PAGEDIR_H

#include <stdbool.h>
#include <stdint.h>

uint_t* pagedir_create(void);
void pagedir_destroy(uint_t* pd);
bool pagedir_set_page(uint_t* pd, void* upage, void* kpage, uint_t rwx);
void* pagedir_get_page(uint_t* pd, const void* upage);
void pagedir_clear_page(uint_t* pd, void* upage);
bool pagedir_is_dirty(uint_t* pd, const void* upage);
void pagedir_set_dirty(uint_t* pd, const void* upage, bool dirty);
bool pagedir_is_accessed(uint_t* pd, const void* upage);
void pagedir_set_accessed(uint_t* pd, const void* upage, bool accessed);
void pagedir_activate(uint_t* pd);
uint_t* active_pd(void);

#endif /* userprog/pagedir.h */
