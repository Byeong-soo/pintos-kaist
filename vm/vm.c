/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include <string.h>
#include <round.h>
#include <stdio.h>
#include "../include/userprog/process.h"

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

	// if(type != VM_MARKER_0)
	ASSERT (VM_TYPE(type) != VM_UNINIT);

	struct supplemental_page_table *spt = &thread_current ()->spt;
	// printf("ininin initializer!!\n");
	// printf("alloc va = %X\n",upage);
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
		
		// printf("new_page va= %X\n",upage);
		// printf("new_page writable = %d\n",writable);
		// printf("new_page writable = %d\n",new_page->writeable);
		if(new_page == NULL){
			// printf("new_page NUL!!!\n");
			goto err;
		}

		// printf("befoe ififif\n");
		// printf("type = %d\n",type);
		if(type == VM_ANON){
			uninit_new(new_page,upage,init,type,aux,anon_initializer);
			new_page->is_stack = false;
		}else if(type == VM_STACK){
			uninit_new(new_page,upage,init,type,aux,NULL);
			new_page->is_stack = true;
		}else if(type == VM_FILE){
			uninit_new(new_page,upage,vm_file_init,type,aux,file_backed_initializer);
			new_page->is_stack = false;
		}
		new_page->writable = writable;
		// }else if(type == VM_MARKER_0){
		// 	uninit_new(new_page,upage,,type,NULL,NULL);
		// }

		// printf("new_page va = %X\n",upage);
		// printf("new_page writable = %d\n",new_page->writable);
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
		// struct page_table_node * find_page = list_entry(page_list_elem,struct page_table_node, elem);
		struct page * find_page = list_entry(page_list_elem,struct page, elem);
		// if(find_page->page->va == va){
		if(find_page->va == va){
			// printf("find va = %X\n",va);
			// printf("tread current = %d\n",thread_current()->tid);
			return find_page;
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

	// struct page_table_node* new_page_node = (struct page_table_node *)malloc(sizeof(struct page_table_node));

	// if(new_page_node == NULL || page == NULL){
	if(page == NULL){
		return false;
	}
	// new_page_node->page = page;
	// printf("push va = %X\n",page->va);
	list_push_front(&spt->page_list,&page->elem);
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
		// struct page_table_node * find_page_node = list_entry(page_list_elem,struct page_table_node, elem);
		struct page * find_page = list_entry(page_list_elem,struct page, elem);
		
		if(find_page == page){
			list_remove(page_list_elem);
			vm_dealloc_page (page);
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

	if(frame == NULL || frame->kva == NULL){
		PANIC("TODO");
	}
	
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr) {
	bool success = false;
	// uint64_t new_stack_bottom = addr - PGSIZE;
	struct thread *t = thread_current();
	uint64_t cur_thread_bottom = t->spt.stack_bottom;

	do
	{
		cur_thread_bottom = cur_thread_bottom - PGSIZE;
		vm_alloc_page(VM_STACK,cur_thread_bottom,true);
		success = vm_claim_page(cur_thread_bottom);
		if(!success){
			cur_thread_bottom = cur_thread_bottom + PGSIZE;
			break;
		}
	} while (cur_thread_bottom != addr);
	
	t->spt.stack_bottom = cur_thread_bottom;
	// t->spt.stack_bottom = new_stack_bottom;
	// vm_claim_page(new_stack_bottom);
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
	struct thread * t = thread_current();
	struct supplemental_page_table *spt = &t->spt;

	struct page *page = NULL;
	uint64_t addrs = pg_round_down(addr);

	// printf("stack bottom = %X\n",spt->stack_bottom);
	// printf("thread_current() %d\n",thread_current()->tid);
	// printf("addr = %X\n",addrs);
	// printf("USER STACK - addr = %X\n",USER_STACK - (uint64_t)addr);
	// printf("rsp = %X\n",f->rsp);
	// printf("USER STACK - rsp = %X\n",USER_STACK - f->rsp);
	// printf("writable !!!!!= %d\n",write);
	
	// TODO: Validate the fault

	page = spt_find_page(spt,addrs);
	// USER_STACK
	if(page == NULL){
		if(user){
			// if(spt->stack_bottom - PGSIZE < addr && addr < spt->stack_bottom){
				// printf("addr pg_down = %X\n",addrs);
				// printf("USER_STACK-PGSIZE - addr = %X\n",(USER_STACK-PGSIZE) - addrs);
			if((spt->stack_bottom > f->rsp) && ((USER_STACK-PGSIZE) - addrs) <= 0x1000000){
				vm_stack_growth(addrs);
				return true; 
			}
		}
		thread_current()->exit_code = -1;
		thread_exit();
		// printf("thread current = %d\n",thread_current()->tid);
	}
	// TODO: Your code goes here
	// printf("writable !!!!_!!_!_!_!_= %d\n",page->writable);
	// printf("enter vm_do_claim_page\n");
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
	struct thread* t = thread_current();
	// TODO: Fill this function
	// page = (struct page*)malloc(sizeof(struct page));
	// page->va = va; 
	// vm_alloc_page(VM_ANON,va,true);
	// page->is_stack = true;
	// page->writable = true;

	// vm_alloc_page(VM_MARKER_0,va,true);
	page = spt_find_page(&t->spt,va);
	// printf("page va^^^^^^ =%X\n",page->va);
	// ASSERT(page == NULL);
	// if(page.)
	// vm_alloc_page(1,va,true);
	// printf("page va = %X\n",page->va);
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {

	if(pml4_get_page(thread_current()->pml4, page->va) != NULL){
		// printf("pml4_get_page is not null!!!!!!!!!!!!!!!!!!!!!\n");
		return false;
	}
	// printf("pml4_get_page is NULL\n");

	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;


	// TODO: Insert page table entry to map page's VA to frame's PA.
	bool success = true;
	// hex_dump(page,page,sizeof(struct page),true);
	// printf("!!!!!!!!!!!!!!!!page va before swap = %X\n",page->va);
	// printf("page writable before swap = %d\n",page->writable);
	// printf("frame kva = %X\n",frame->kva);
	success = pml4_set_page(thread_current()->pml4,page->va,frame->kva,page->writable);
	if(!success){
		return false;
	}

	if(page->is_stack == true){
		return true;
	}
	// printf("enter swap_in\n");
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

	struct list *p_spt_list, *c_spt_list;
	struct page * copy_page;
	struct file *copy_file;

	bool success = true;
	
	p_spt_list = &src->page_list;
	c_spt_list = &dst->page_list;

	if (list_empty(p_spt_list))
		return true;

	struct list_elem *cur;

	cur = list_begin(p_spt_list);
	while (cur != list_end(p_spt_list))
	{
		copy_page = list_entry(cur, struct page, elem);
		if(copy_page != NULL){
			// printf("copy spt\n");
			// printf("copy va = %X\n",copy_page_node->page->va);

			enum vm_type type = copy_page->uninit.type;

			if(copy_page->uninit.type == VM_FILE){
				cur = list_next(cur);
				continue;
			}

			void * va = copy_page->va;
			vm_initializer *init = copy_page->uninit.init;
			void *aux = copy_page->uninit.aux;
			bool is_stack = copy_page->is_stack;

			struct page * new_page = (struct page*)malloc(sizeof(struct page));
			struct load_info * new_aux = (struct load_info*)malloc(sizeof(struct load_info));

			if(aux != NULL){
				memcpy(new_aux,aux,sizeof(struct load_info));
			}
			if(new_page == NULL){
				return false;
			}

			if(type == VM_ANON){
				uninit_new(new_page,va,init,type,new_aux,anon_initializer);		
			}else if(type == VM_STACK){
				uninit_new(new_page,va,NULL,type,NULL,anon_initializer);
			}

			// printf("origin page va= %X\n",copy_page_node->page->va);
			// printf("copy page va= %X\n",new_page->va);
			// printf("origin frame kva = %X\n",copy_page_node->page->frame->kva);
			//! TODO 오류 처리
			spt_insert_page(dst,new_page);

			new_page->writable = copy_page->writable;
			new_page->is_stack = is_stack;
			if(copy_page->frame != NULL){
				vm_do_claim_page(new_page);
				memcpy(new_page->frame->kva,copy_page->frame->kva,PGSIZE);
			}
			// printf("copy frame kva = %X\n",new_page->frame->kva);

			// printf("in copy writable = %d\n",copy_page_node->page->writable);
			// printf("copyed new page va = %X\n",new_page->va);
			// printf("in copyed new page writable = %d\n",new_page->writable);
			// printf("result success = %d\n",success);
		}
		cur = list_next(cur);
	}

	return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	// TODO: Destroy all the supplemental_page_table hold by thread and
	// TODO: writeback all the modified contents to the storage.

	struct list* delete_list = &spt->page_list;	
	struct list_elem * delete_elem = list_begin(delete_list);
	struct page * delete_page;
	struct frame * delete_frame;
	char index = 0;
	if(list_empty(delete_list) || delete_elem == NULL){
		return;
	}


	while (delete_elem != list_end(delete_list))
	{	
		// delete_page_node = list_entry(delete_elem, struct page_table_node, elem);
		delete_page = list_entry(delete_elem, struct page, elem);
		if(delete_page->uninit.type == VM_FILE){
			struct load_info * info = (struct load_info *)delete_page->file.aux;
			delete_elem = list_next(delete_elem);
			do_munmap(delete_page->va);
			continue;
		}
		delete_elem = list_remove(delete_elem);
		vm_dealloc_page(delete_page);
		// free(delete_page->uninit.aux);
		// if(delete_page->frame != NULL){
		// 	free(delete_page->frame);
		// }
		// free(delete_page);
	}
}
