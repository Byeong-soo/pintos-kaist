/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include "lib/kernel/list.h"
#include <string.h>
#include <round.h>
#include <stdio.h>
#include "../include/userprog/process.h"
#include "lib/kernel/bitmap.h"
#include "threads/synch.h"

struct list frame_list;
struct bitmap *disk_bitmap;
struct lock bitmap_lock,frame_lock;
/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
	list_init(&frame_list);
	lock_init(&bitmap_lock);
	lock_init(&frame_lock);
	disk_bitmap = setup_swap_disk_bitmap();
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


		if(type == VM_ANON){
			uninit_new(new_page,upage,init,type,aux,anon_initializer);
			new_page->is_stack = false;
		}else if(type == VM_STACK){
			uninit_new(new_page,upage,init,type,aux,NULL);
			new_page->is_stack = true;
		}else if(type == VM_FILE){
			uninit_new(new_page,upage,init,type,aux,file_backed_initializer);
			new_page->is_stack = false;
		}



		//! TODO 오류 처리

		// if(new_page->is_stack == true){
		// 	spt_insert_page(spt,new_page,&spt->stack_list);
		// }else{
		
		spt_insert_page(spt,new_page,&spt->page_list);
		new_page->writable = writable;
		new_page->swap_bit_index = SWAP_BIT_DEFAULT;
		new_page->tid = thread_current()->tid;
		// printf("va %X  new page bit = %d tid = %d\n",new_page->va,new_page->swap_bit_index,new_page->tid);
		// }
	}
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

	// page_list = &spt->stack_list;
	page_list_elem = list_begin(page_list);

	while (page_list_elem != list_end(page_list)){
		struct page * find_page = list_entry(page_list_elem,struct page, elem);
		if(find_page->va == va){
			return find_page;
		}
		page_list_elem = list_next(page_list_elem);
		return NULL;
	} 
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt, struct page *page,struct list * list) {
	/* TODO: Fill this function. */
	int succ = false;

	list_push_front(list,&page->elem);
	return true;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page,struct list * list) {

	struct list * page_list;
	struct list_elem * page_list_elem;

	page_list = list;
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
	struct list_elem *victim_elem = NULL;
	 // TODO: The policy for eviction is up to you.

	frame_lock_aquire();
	// victim_elem = list_begin(&frame_list);

	


	while (1)
	{	
		if(list_empty(&frame_list)){
			return NULL;
		}

		bool check = true;

		victim_elem = list_pop_front(&frame_list);

		victim = list_entry(victim_elem,struct frame,elem);

		if(list_empty(&victim->page_list)){
			list_push_back(&frame_list,&victim->elem);
			continue;
		}


		struct list_elem * curr = list_begin(&victim->page_list);	
		// printf("page count !! =%d\n",list_size(&victim->page_list));
		// printf("first bool = %d\n",check);
		struct page *victim_page = list_entry (curr, struct page, frame_list_elem);		

		if(!victim_page->writable || victim_page->frame->kva == NULL || victim_page->uninit.type == VM_STACK){
			check = false;
		}

		list_push_back(&frame_list,&victim->elem);

		// printf("second bool = %d\n",check);
		// printf("frame kva = %X page va = %X page writable = %d\n",victim->kva,victim_page->va,victim_page->writable);
		if(check){break;}
	}
	frame_lock_release();
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {

	// printf("before get victim \n");
	struct frame *victim = vm_get_victim ();
	
	// TODO: swap out the victim and return the evicted frame.

	struct list_elem *curr =list_begin(&victim->page_list);
	struct page * victim_page = list_entry (curr, struct page, frame_list_elem);
	// printf("after get victim va = %X page va %X\n ",victim->kva,victim_page->va);

	swap_out(victim_page);
	// printf("out swap out\n");
	// while (curr != list_end(&victim->page_list)) {
		// printf("victim page va = %X page type = %d\n",victim_page->va,victim_page->uninit.type);
		// printf("victim kva = %X\n",victim->kva);
		// curr = list_next(curr);
	// }
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {

	frame_lock_aquire();


	//! free !!
	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));

	if(frame == NULL){
		frame = vm_evict_frame();
	}


	//TODO: Fill this function.
	frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);
	// frame->page = NULL;

	if(frame->kva == NULL){
		free(frame);
		// printf("before in vm_evict_frame\n");
		frame = vm_evict_frame();
		// frame->page = NULL;
	}else{
		list_init(&frame->page_list);
		list_push_back(&frame_list,&frame->elem);
	}

	// printf("out in vm_evict_frame\n");


	ASSERT (frame != NULL);

	ASSERT (list_empty(&frame->page_list));
	frame_lock_release();
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
	frame_lock_aquire();
	// printf("==============in vm handle wp ===============\n");
	// printf("copy memory!!\n");
	// printf("thread_current = %d\n",thread_current()->tid);
	struct frame * frame,*new_frame,*pre_frame;
	struct page * pre_page;
	void * kva;
	kva = page->frame->kva;
	frame = page->frame;
	
	// 페이지가 쓰기가능. 근데 falut 보호 받고 있음.
	if(page->writable == true){
		
		// 원래 매모리 해제
		// struct hash_elem * delete =  hash_delete(&page->frame->page_hash,&page->frame_hash_elem);
		
		// if(hash_size(&frame->page_hash) == 1){

		// 	struct hash_iterator i;
		// 	hash_first (&i, &frame->page_hash);
		// 	hash_next(&i);

		// 	struct page *left_page = hash_entry (hash_cur (&i), struct page, frame_hash_elem);
		// 	if(!pml4_set_page(&left_page->pml4,left_page->va,left_page->frame->kva,left_page->writable)){
		// 		PANIC("faile set_page");
		// 	}

		// 	// left_page->cow = false;
		// 	printf("left page va = %X, left page kva = %X left page cow = %d left_pml4 = %d tid = %d\n",left_page->va,left_page->frame->kva,left_page->cow,left_page->pml4,left_page->tid);
		// }

		// struct page *delete2 = hash_entry (delete, struct page, frame_hash_elem);
		// printf("delete page va = %X, left page kva = %X left page cow = %d  left_pml4 = %d tid = %d\n",delete2->va,delete2->frame->kva,delete2->cow,delete2->pml4,delete2->tid);


		pml4_clear_page(thread_current()->pml4,page->va);

		vm_do_claim_page(page);
		struct page * find_page = spt_find_page(&thread_current()->spt,page->va);

		// printf("page va = %X frame kva = %X \n",page->va,page->frame->kva);
		// printf("find page va = %X frame kva = %X \n",find_page->va,find_page->frame->kva);
		// printf("find_page writable = %d\n",find_page->writable);


		memcpy(page->frame->kva,frame->kva,PGSIZE);
		page->cow = false;
		frame_lock_release();
	// printf("===================================================\n");
		return true;
	}
	return false;
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f , void *addr ,
		bool user , bool write , bool not_present ) {
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
	// if(write == true){
		// printf("write = %d\n",write);
		// printf("page cow = %d\n",page->cow);

	// 	return vm_handle_wp (page);
	// }
	// printf("===============handler_fault===============\n \
	// // thread tid %d\n",thread_current()->tid);
	// if(page->frame != NULL){
	// 	printf("page va = %X . page frame kva = %X\n",page->va,page->frame->kva);
	// }else{
	// 	printf("page va = %X \n",page->va);
	// }
	// printf("page cow = %d write %d page writable %d \n\n\n",page->cow,write,page->writable);


	if(page->cow == true && write == true && page->writable == true){	
		return vm_handle_wp (page);
	}


	// TODO: Your code goes here
	if(page->writable == false && write == true){
		return false;
	}
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

	if(page == NULL){
		return false;
	}
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

	// if(pml4_get_page(thread_current()->pml4, page->va) != NULL){
	// 	// printf("pml4_get_page is not null!!!!!!!!!!!!!!!!!!!!!\n");
	// 	return false;
	// }
	// printf("pml4_get_page is NULL\n");

	// printf("!!!!!!!!!!!!!!!!page va before swap = %X\n",page->va);
	// printf("before vm_get frame");
	struct frame *frame = vm_get_frame ();
	// printf("after vm_get frame");
	/* Set links */
	// frame->page = page;
	frame_lock_aquire();
	list_push_back(&frame->page_list,&page->frame_list_elem);
	frame_lock_release();

	page->frame = frame;
	// printf("page va = %X   get frame  %X!!!!!!!\n",page->va,page->frame->kva);
	page->cow = false;
	// printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	// printf("page va = %X\n",page->va);
	// printf("frame kva = %X\n",frame->kva);
	// PANIC("hi?");

	// TODO: Insert page table entry to map page's VA to frame's PA.
	bool success = true;
	// hex_dump(page,page,sizeof(struct page),true);
	// printf("page writable before swap = %d\n",page->writable);
	// printf("frame kva = %X\n",frame->kva);

	// if(page->uninit.type == VM_STACK || page->uninit.type == VM_FILE){
	// 	success = pml4_set_page(thread_current()->pml4,page->va,frame->kva,page->writable);
	// }else if(page->cow == true){

	success = pml4_set_page(thread_current()->pml4,page->va,frame->kva,page->writable);
	// }else{
	// 	success = pml4_set_page(thread_current()->pml4,page->va,frame->kva,false);
	// }

	if(!success){
		PANIC("pml4 set page fail");
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
	spt->pml4 = t->pml4;
	list_init(&t->spt.page_list);
	list_init(&t->spt.stack_list);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src) {

	// printf("\n\n");
	// printf("========copy spt======== thrad_ tid = %d\n\n",thread_current()->tid);
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


			//! TODO 오류 처리
			spt_insert_page(dst,new_page,&dst->page_list);

			new_page->writable = copy_page->writable;
			new_page->is_stack = is_stack;
			new_page->swap_bit_index = SWAP_BIT_DEFAULT;

			if(copy_page->frame != NULL){
				if(copy_page->uninit.type == VM_STACK){
					vm_do_claim_page(new_page);
					memcpy(new_page->frame->kva,copy_page->frame->kva,PGSIZE);
					cur = list_next(cur);
					// printf("!!!!origin frame kva = %X , origin page va = %X\n",copy_page->frame->kva,copy_page->va);
					// printf("!!!!copy frame kva = %X, copyed new page va = %X\n",new_page->frame->kva,new_page->va);
					continue;
				}

				new_page->frame = copy_page->frame;
				new_page->cow = true;
				copy_page->cow = true;
				new_page->pml4 = thread_current()->pml4;
				new_page->tid = thread_current()->tid;
				// new_page->frame->cow_count +=1;
				frame_lock_aquire();
				list_push_back(&copy_page->frame->page_list,&new_page->frame_list_elem);
				// hash_insert(&copy_page->frame->page_hash,&new_page->frame_hash_elem);
				frame_lock_release();

				// printf("page va = %X after copy hash size = %d\n",copy_page->va,list_size(&copy_page->frame->page_list));
				// printf("frame kva = %X \n",copy_page->frame->kva);
				if(!pml4_set_page(thread_current()->pml4,new_page->va,copy_page->frame->kva,false)){
					PANIC("copy pml4 fail");
					return false;
				}
				// printf("copy pml4 set page %d\n",su);
				pml4_clear_page(src->pml4,copy_page->va);

				if(!pml4_set_page(src->pml4,copy_page->va,copy_page->frame->kva,false)){
					PANIC("copy origin pml4 fail");
					return false;
				}

				// if(!success && copy_page->writable == true){
				// 	pml4_clear_page(thread_current()->pml4, new_page->va);
				// 	vm_do_claim_page(new_page);
				// 	memcpy(new_page->frame->kva,copy_page->frame->kva,PGSIZE);
				// 	pml4_set_page(thread_current()->pml4,new_page->va,new_page->frame->kva,false);

				// 	// printf("su = %d\n",su);
				// }
				// printf("------------------------------------\n");
				// printf("copy_page va = %X copy_page kva =%X \n",copy_page->va,copy_page->frame->kva);
				// printf("pml4_set_page success = %d\n",success);
			}

				// struct frame *new_frame = (struct frame *)malloc(sizeof(struct frame));
				// new_page->frame = new_frame;
				// new_frame->page = new_page;

				// new_page->frame->kva = copy_page->frame->kva;
				// printf("cow count= %X\n",copy_page->frame->cow_count);
				// printf("cow frame va = %X\n",copy_page->frame->kva);
				// printf("cow count %d\n",copy_page->frame->cow_count);
				// memcpy(new_page->frame->kva,copy_page->frame->kva,PGSIZE);
				// new_page->frame = copy_page->frame;
				// printf("origin frame kva = %X , origin page va = %X\n",copy_page->frame->kva,copy_page->va);
				// printf("copy frame kva = %X, copyed new page va = %X\n",new_page->frame->kva,new_page->va);

			// if(copy_page->frame != NULL){
			// 	vm_do_claim_page(new_page);
			// // printf("current threrad = %d\n",thread_current()->tid);
			// 	// printf("new page va = %X\n",new_page->va);
			// 	memcpy(new_page->frame->kva,copy_page->frame->kva,PGSIZE);
			// }

			// printf("in copy writable = %d\n",copy_page_node->page->writable);
			// printf("in copyed new page writable = %d\n",new_page->writable);
			// printf("result success = %d\n",success);
		}
		cur = list_next(cur);
	}
// printf("========copy spt end========\n\n");
	return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	// TODO: Destroy all the supplemental_page_table hold by thread and
	// TODO: writeback all the modified contents to the storage.

	struct list* delete_list = &spt->page_list;	

	if(list_empty(delete_list)){
		return;
	}
	// struct list_elem * delete_elem = list_pop_front(delete_list);
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
	}
}

size_t
find_empty_disk_sector(){
	bitmap_lock_aquire();
	size_t index = bitmap_scan_and_flip(disk_bitmap,0,1,false);
	bitmap_lock_release();
	return index;
}

void
restore_bitmap(size_t index){
	bitmap_lock_aquire();
	bitmap_flip(disk_bitmap,index);
	bitmap_lock_release();
}

void
bitmap_lock_aquire(){
	if(!lock_held_by_current_thread(&bitmap_lock)){
		lock_acquire(&bitmap_lock);
	}
}

void
bitmap_lock_release(){
	if(lock_held_by_current_thread(&bitmap_lock)){
			lock_release(&bitmap_lock);
		}
}

void
frame_lock_aquire(){
	if(!lock_held_by_current_thread(&frame_lock)){
		lock_acquire(&frame_lock);
	}
}


void
frame_lock_release(){
	if(lock_held_by_current_thread(&frame_lock)){
		lock_release(&frame_lock);
	}
}

// unsigned
// page_hash (const struct hash_elem *p_, void *aux UNUSED) {
//   const struct page *p = hash_entry (p_, struct page, frame_hash_elem);
//   return hash_bytes (&p->tid ,sizeof p->tid);
// }