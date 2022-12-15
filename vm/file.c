/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/mmu.h"
#include "userprog/process.h"
#include <stdio.h>
#include <string.h>
#include "userprog/syscall.h"


static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

struct load_info_mmu
{
	uint64_t page_read_bytes;
	uint64_t page_zero_bytes;
	off_t offset;
	struct file *file;
	uint64_t va;
	uint64_t start_va;
};


/* The initializer of file vm */
void
vm_file_init (void) {

}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */

	vm_initializer * init = page->uninit.init;
	struct load_info * load_info = page->uninit.aux;

	page->operations = &file_ops;
	struct file_page *file_page = &page->file;

	file_page->init = init;
	file_page->type = type;
	file_page->aux = load_info;

	return (init ? init (page, load_info) : false);	
	// return true;	
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	// printf("file swap in ! \n");
	bool has_lock = false;
	bool success = false;
	has_lock = check_file_lock_holder();


	struct load_info_mmu *lazy_info = (struct load_info_mmu *) page->file.aux;
	
	if(!has_lock){file_lock_acquire();}
	file_seek(lazy_info->file,lazy_info->offset);

	if (file_read(lazy_info->file, kva, lazy_info->page_read_bytes) != (int)lazy_info->page_read_bytes)
	{ 
		if(!has_lock){file_lock_release();}
		palloc_free_page(page);
		return false;
	}
	memset(kva+ lazy_info->page_read_bytes, 0, lazy_info->page_zero_bytes);
	if(!has_lock){file_lock_release();}
	// printf("file swap out ! \n");
	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page = &page->file;

	struct thread * t = thread_current();
	struct list * page_list = &t->spt.page_list;
	struct list_elem * del_elem;

	struct load_info_mmu * new_aux = (struct load_info_mmu *)page->file.aux;

	if(pml4_is_dirty(t->pml4,page->va) && page->writable){
		file_lock_acquire();
		file_write_at(new_aux->file,page->frame->kva,new_aux->page_read_bytes,new_aux->offset);
		file_close(new_aux->file);
		file_lock_release();
	}
	
	pml4_set_dirty(t->pml4,page->va,false);
	pml4_clear_page(t->pml4,page->va);
	page->frame->kva = NULL;
	return true;

	error:
	return false;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page = &page->file;
	struct thread * t = thread_current();
	if(page->uninit.aux != NULL){
		free(page->uninit.aux);
	}

	if(page->frame != NULL){
		free(page->frame);
	}
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {

	off_t return_value;
	uint64_t start_va,end_va;
	size_t check_offset = offset;
	start_va = pg_round_down(addr);
	end_va = pg_round_down(addr + length -1);
	struct page *start_page,*end_page;

	start_page = spt_find_page(&thread_current()->spt,start_va);
	end_page = spt_find_page(&thread_current()->spt,end_va);

	if(is_kernel_vaddr(start_va) || is_kernel_vaddr(end_va))
		return NULL;
 
	file_lock_acquire();
	size_t file_len = file_length(file);
	file_lock_release();

	if(start_page == NULL || end_page == NULL){
		
		while (start_va < end_va + PGSIZE)
		{
			if(!pml4_get_page(thread_current()->pml4,start_va)){
				struct load_info_mmu * new_aux = (struct load_info_mmu*)malloc(sizeof(struct load_info_mmu));

				file_lock_acquire();
				new_aux->file = file_reopen(file);
				file_lock_release();
				
				new_aux->offset = check_offset;
				new_aux->va = start_va;
				new_aux->start_va = addr;
				if(file_len < PGSIZE){
					new_aux->page_read_bytes = file_len;
					new_aux->page_zero_bytes = PGSIZE - file_len;
				}else{
					new_aux->page_read_bytes = PGSIZE;
					new_aux->page_zero_bytes = 0;
				}

				if(!vm_alloc_page_with_initializer(VM_FILE,start_va,writable,mmap_lazy_load,new_aux)){
					return NULL;
				}
				file_len -=PGSIZE;
				check_offset += PGSIZE;
			}
			start_va += PGSIZE;
		}
	}else{
		return NULL;
	}
	void * mmap = NULL;
	mmap = addr;
	return mmap;
}

/* Do the munmap */
void
do_munmap (void *addr) {

	struct thread * t = thread_current();
	struct page * page;
	struct list * page_list = &t->spt.page_list;
	struct list_elem * del_elem;

	del_elem = list_begin(page_list);

	if(list_empty(page_list) || del_elem== NULL){
		return;
	}

	while (del_elem != list_end(page_list))
	{
		page = list_entry(del_elem,struct page,elem);
		if(page->uninit.type == VM_FILE){
			struct load_info_mmu * new_aux = (struct load_info_mmu *)page->file.aux;
			if(new_aux->start_va == addr){
				if(pml4_is_dirty(t->pml4,page->va) && page->writable){
					
					file_lock_acquire();
					file_write_at(new_aux->file,page->frame->kva,new_aux->page_read_bytes,new_aux->offset);
					file_close(new_aux->file);
					file_lock_release();
				}
				del_elem = list_remove(del_elem);
				vm_dealloc_page(page);
				continue;
			}
		}
		del_elem = list_next(del_elem);
	}
}

bool
mmap_lazy_load(struct page *page, void *aux)
{	
	// printf("enter mmap_lazy_load!!!!!!\n");
	bool has_lock = false;
	bool success = false;
	has_lock = check_file_lock_holder();

	struct load_info_mmu *lazy_info = (struct load_info_mmu *) aux;
	
	if(!has_lock){file_lock_acquire();}
	file_seek(lazy_info->file,lazy_info->offset);
 
	if (file_read(lazy_info->file, page->frame->kva, lazy_info->page_read_bytes) != (int)lazy_info->page_read_bytes)
	{ 
		// PANIC("sadfasdf");
		if(!has_lock){file_lock_release();}
		palloc_free_page(page);
		return false;
	}
	memset(page->frame->kva + lazy_info->page_read_bytes, 0, lazy_info->page_zero_bytes);
	if(!has_lock){file_lock_release();}
	// printf("out mmap_lazy_load!!!!!!\n");
	return true;
}