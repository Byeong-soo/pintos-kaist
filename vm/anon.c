/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/mmu.h"
#include <stdio.h>
#include "lib/kernel/bitmap.h"

//! DO NOT MODIFY BELOW LINE
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

//! DO NOT MODIFY this struct
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void vm_anon_init (void) {
	// TODO: Set up the swap_disk.
	swap_disk = disk_get(1,1);

}

struct bitmap*
setup_swap_disk_bitmap(){
	disk_sector_t total_size = disk_size(swap_disk);
	return bitmap_create((total_size)/8);
}



/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */

	page->operations = &anon_ops;
	struct anon_page *anon_page = &page->anon;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;

	if(page->swap_bit_index == NULL){
		return true;
	}

	for(int i = 0; i < 8; i++){
		disk_read(swap_disk,(page->swap_bit_index * 8) + i,kva + (i * DISK_SECTOR_SIZE));
	}

	restore_bitmap(page->swap_bit_index);
	page->swap_bit_index = NULL;
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {

	struct anon_page *anon_page = &page->anon;
	struct thread * t = thread_current();

	size_t index = find_empty_disk_sector();
	if(index == BITMAP_ERROR){
		return false;
	}

	for(int i = 0; i < 8; i++){
		disk_write(swap_disk,(index * 8) + i, page->frame->kva + (i * DISK_SECTOR_SIZE));
	}

	page->swap_bit_index = index;
	struct frame * swap_out_frame = page->frame;

	pml4_clear_page(t->pml4,page->va);
	swap_out_frame->page = NULL;
	page->frame = NULL;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	if(page->swap_bit_index != NULL){
		restore_bitmap(page->swap_bit_index);
	}

	if(page->uninit.aux != NULL){
		free(page->anon.aux);
	}
	
	if(page->frame != NULL){
		free(page->frame);
	}
}
