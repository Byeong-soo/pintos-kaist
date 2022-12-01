/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"

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
	printf("before spt_find_page\n");
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {

		printf("after spt_find_page\n");
		// TODO: Create the page, fetch the initialier according to the VM type,
		// TODO: and then create "uninit" page struct by calling uninit_new. You
		// TODO: should modify the field after calling the uninit_new.
		// TODO: Insert the page into the spt.

		//! free!!
		struct page_table_node * new_page = (struct page_table_node *)malloc(sizeof(struct page_table_node));
		uint8_t *kpage;
		void * va = upage;

		kpage = palloc_get_page(PAL_USER);
		struct page * page = kpage;
		if (kpage == NULL)
			return false;

		uninit_new(page,va,init,type,aux,page->operations->swap_in);
		list_push_back(&spt->page_list,&new_page->elem);
		// printf("after uninit!\n");
		// TODO va를 입력?
		// spt->type = type;
		// if(type == VM_ANON){
// 
			// struct anon_page anon;
			// anon = spt->page->anon;
			// spt->page = kpage;
			// spt->type = VM_ANON;
			// spt->page->anon.writable = writable;
			// printf("anon!!!\n");
		// }else if(type == VM_FILE){
// 
			// struct file_page file;
			// file = spt->page->file;
			// printf("file!!!\n");
		// }

	}
	// printf("vm_alloc_page_with_initializer end!!!!!\n");
	return true;
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	printf("enter spt_find\n");
	printf("va x= %x\n",va);
	printf("va p= %p\n",va);
	printf("va d= %d\n",va);
	struct page *page = NULL;
	/* TODO: Fill this function. */
	struct list * page_list;
	struct list_elem * page_list_elem;
	printf("spt -> page_list before\n");
	page_list = &spt->page_list;
	page_list_elem = list_begin(page_list);

	while (page_list_elem != list_end(page_list))
	{
		struct page_table_node * find_page = list_entry(page_list_elem,struct page_table_node, elem);
		printf("before check va\n");
		if(find_page->page->va == va){
			printf("return find page\n");
			return find_page->page;
		}
		printf("after check va\n");
		page_list_elem = list_next(page_list_elem);
	}
	printf("spt_find_page_return_null\n");
	return NULL;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt, struct page *page) {
	int succ = false;
	/* TODO: Fill this function. */

	struct page_table_node* new_page = (struct page_table_node *)malloc(sizeof(struct page_table_node));
	// page = palloc_get_page(PAL_USER);
	new_page->page = page;
	
	spt->page_list;

	return succ;
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
	struct frame *frame = NULL;
	//TODO: Fill this function.
	// frame = palloc_get_page(PAL_USER);
	// frame = vtop(frame);
	// frame->kva = frame;
	PANIC("todo");	

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
	
	// TODO: Validate the fault
	// printf("fault addr = %x\n",addr);
	// printf("user bool = %d\n",user);
	// printf("write bool = %d\n",write);
	// printf("not_present bool = %d\n",not_present);
	// page = spt->page;
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
	struct frame *frame = vm_get_frame ();
	// printf("vm_do_claim_page get frame\n");

	// printf("vm_do_claim_page\n");

	/* Set links */
	frame->page = page;
	page->frame = frame;

	// TODO: Insert page table entry to map page's VA to frame's PA.

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
