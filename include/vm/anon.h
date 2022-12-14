#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
struct page;
enum vm_type;

struct anon_page {
    vm_initializer *init;
	enum vm_type type;
	void *aux;

};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);
struct bitmap* setup_swap_disk_bitmap();

#endif
