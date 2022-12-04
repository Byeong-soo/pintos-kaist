/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include <round.h>
#include <stdio.h>

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	//! DO NOT MODIFY UPPER LINES.
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		// printf("upage2 = %x\n",upage);

		// TODO: Create the page, fetch the initialier according to the VM type,
		// TODO: and then create "uninit" page struct by calling uninit_new. You
		// TODO: should modify the field after calling the uninit_new.
		// TODO: Insert the page into the spt.

		//! free!!
		// struct page_table_node * new_page_node = (struct page_table_node *)malloc(sizeof(struct page_table_node));
		//! free!!
		struct page * new_page = (struct page*)malloc(sizeof(struct page));
		new_page->writeable = writable;

		if(new_page == NULL){
			goto err;
		}

		// printf("uninit_page size = %d\n",sizeof(struct uninit_page));
		// printf("kpage = %x\n",kpage);
		// if (kpage == NULL){
		// 	return false;
		// }


		// printf("new_page _init %d\n",new_page->uninit.init);
		// list_push_back(&spt->page_list,&new_page->elem);

		// spt->type = type;
		if(type == VM_ANON){
			uninit_new(new_page,upage,init,type,aux,anon_initializer);
			// printf("anon!!!\n");

		}else if(type == VM_FILE){
			struct file_page file;
		}

		//! TODO 오류 처리
		spt_insert_page(spt,new_page);
		// printf("hi!!!!!!!!!!!!@@@@@@@@@@@@@@@\n");

	}
	// printf("vm_alloc_page_with_initializer end!!!!!\n");
	return true;
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	/* TODO: Fill this function. */
	struct page *page = NULL;
	struct list * page_list;
	struct list_elem * page_list_elem;

	page_list = &spt->page_list;
	page_list_elem = list_begin(page_list);

	while (page_list_elem != list_end(page_list))
	{
		struct page_table_node * find_page = list_entry(page_list_elem,struct page_table_node, elem);
		if(find_page->page->va == va){
			return find_page->page;
		}
		page_list_elem = list_next(page_list_elem);
	}
	return NULL;
} 

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt, struct page *page) {
	int succ = false;
	/* TODO: Fill this function. */

	struct page_table_node* new_page_node = (struct page_table_node *)malloc(sizeof(struct page_table_node));

	if(new_page_node == NULL){
		return false;
	}

	new_page_node->page = page;
	list_push_front(&spt->page_list,&new_page_node->elem);
	return true;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {

	struct list * page_list;
	struct list_elem * page_list_elem;

	page_list = &spt->page_list;
	page_list_elem = list_begin(page_list);

	while (page_list_elem != list_end(page_list))
	{
		struct page_table_node * find_page_node = list_entry(page_list_elem,struct page_table_node, elem);
		
		if(find_page_node->page == page){
			list_remove(page_list_elem);
			vm_dealloc_page (page);
			free(find_page_node);
			return true;
		}
		page_list_elem = list_next(page_list_elem);
	}

	return false;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 // TODO: The policy for eviction is up to you.

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();
	// TODO: swap out the victim and return the evicted frame.

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	//! free !!
	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));

	if(frame == NULL){
		return NULL;
	}

	//TODO: Fill this function.
	frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);
	frame->page = NULL;
	
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f , void *addr ,
		bool user , bool write , bool not_present ) {
	// printf("handler_fault\n");

	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = NULL;
	size_t addrs = pg_round_down(addr);

	// TODO: Validate the fault

	page = spt_find_page(spt,addrs);

	if(page == NULL){
		return false;
	}

	// TODO: Your code goes here
	return vm_do_claim_page (page);
}

// Free the page.
//! DO NOT MODIFY THIS FUNCTION.
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va) {
	struct page *page = NULL;
	/* TODO: Fill this function */

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {

	if(pml4_get_page(thread_current()->pml4, page->va) != NULL){
		return false;
	}

	struct frame *frame = vm_get_frame ();
	/* Set links */
	frame->page = page;
	page->frame = frame;

	// TODO: Insert page table entry to map page's VA to frame's PA.
	bool success = true;

	success = pml4_set_page(thread_current()->pml4,page->va,frame->kva,true);
	if(!success){
		return false;
	}

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {

	//* 3주차 추가
	struct thread* t = thread_current();
	list_init(&t->spt.page_list);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	// TODO: Destroy all the supplemental_page_table hold by thread and
	// TODO: writeback all the modified contents to the storage.
}
