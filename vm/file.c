/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/mmu.h"
#include "userprog/process.h"
#include <stdio.h>


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

/* The initializer of file vm */
void
vm_file_init (void) {
	
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page = &page->file;
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
		free(page->frame->kva);
		// pml4_clear_page(t->pml4,page->frame->kva);
		free(page->frame);
	}

	if(page->va != NULL){
		free(page->va);
	}
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {

	off_t return_value;
	uint64_t start_va,end_va;
	size_t check_offset = 0;
	size_t vs_length = length;

	start_va = pg_round_down(addr);
	end_va = pg_round_down(addr + length -1);




	struct page *start_page,*end_page;

	start_page = spt_find_page(&thread_current()->spt,start_va);
	end_page = spt_find_page(&thread_current()->spt,end_va);

	if(start_page == NULL || end_page == NULL){
		
		while (start_va < end_va + 4096)
		{
			if(!pml4_get_page(thread_current()->pml4,start_va)){

				struct load_info * new_aux = (struct load_info*)malloc(sizeof(struct load_info));
				new_aux->file = file;
				new_aux->writable = writable;
				if(vs_length > 4096){
					new_aux->offset = check_offset;
					new_aux->page_read_bytes = 4096;
				}else{
					new_aux->offset = check_offset + vs_length;
					new_aux->page_read_bytes = vs_length;
				}
				new_aux->va = addr;
				
				vm_alloc_page_with_initializer(VM_FILE,start_va,writable,vm_file_init,new_aux);
				struct page * page = spt_find_page(&thread_current()->spt,start_va);
				check_offset += 4096;
				vs_length -= 4096;
			}
			start_va += 4096;
		}
	}
	
	file_lock_acquire();
	file_seek(file,offset);
	return_value = file_read(file,addr,length);
	file_lock_release();

	void * mmap = NULL;
	if(return_value != NULL){
		mmap = addr;
	}

	return addr;
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
	
	while (del_elem != list_tail(page_list))
	{
		page = list_entry(del_elem,struct page,elem);
		struct load_info * new_aux = (struct load_info*)page->uninit.aux;
		if(new_aux != NULL){
			if(new_aux->va == addr){
				// printf("file %X\n",new_aux->file);
				// printf("read_byte = %d\n",new_aux->page_read_bytes);
				// printf("offset = %d\n",new_aux->offset);
				// printf("va = %X\n",new_aux->va);
				// file_lock_acquire();
				// file_write_at(new_aux->file,page->frame->kva,new_aux->page_read_bytes,new_aux->offset);
				// file_lock_release();

				del_elem = list_remove(del_elem);
				vm_dealloc_page(page);
				continue;
			}
		}
		del_elem = list_next(del_elem);
	}
}
