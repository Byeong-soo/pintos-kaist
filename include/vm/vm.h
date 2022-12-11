#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"

enum vm_type {
	/* page not initialized */
	VM_UNINIT = 0,
	/* page not related to the file, aka anonymous page */
	VM_ANON = 1,
	/* page that realated to the file */
	VM_FILE = 2,
	/* page that hold the page cache, for project 4 */
	VM_PAGE_CACHE = 3,

	/* Bit flags to store state */
	VM_STACK = 4,

	/* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */
	VM_MARKER_0 = (1 << 3),
	VM_MARKER_1 = (1 << 4),

	/* DO NOT EXCEED THIS VALUE. */
	VM_MARKER_END = (1 << 31),
};

enum page_position {

	VM_POS_FRAME = 0,
	VM_POS_DISK = 1,
	VM_POS_SWAP = 2,

};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#include "lib/kernel/list.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type) & 7)

/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
struct page {
	const struct page_operations *operations;
	void *va;              /* Address in terms of user space */
	struct frame *frame;   /* Back reference for frame */

	/* Your implementation */
	bool writable;
	bool is_stack;
	struct list_elem elem;
	size_t swap_bit_index;
	bool cow;
	
	/* Per-type data are binded into the union.
	 * Each function automatically detects the current union */
	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};

/* The representation of "frame" */
struct frame {
	void *kva;
	struct page *page;
	struct list_elem elem;
	size_t cow_count;
};

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */
struct page_operations {
	bool (*swap_in) (struct page *, void *);
	bool (*swap_out) (struct page *);
	void (*destroy) (struct page *);
	enum vm_type type;
};

#define swap_in(page, v) (page)->operations->swap_in ((page), v)
#define swap_out(page) (page)->operations->swap_out (page)
#define destroy(page) \
	if ((page)->operations->destroy) (page)->operations->destroy (page)


//* Representation of current process's memory space.
//* We don't want to force you to obey any specific design for this struct.
//? All designs up to you for this.
//TODO
struct supplemental_page_table {
	//페이지 테이블에 접근 불가능 페이지 나타내는 변수.
	//추가된 페이지를 읽거나 쓸 떄 운영체제로 트랩이 발생하는데 이떄 demand zeroing 해줘야하는 페이지임을 알게해줘야함.
	// 트랩이 발생한 시점에 물리페이지를 0으로 채우고 프로세스의 주소 공간으로 매핑하는 등의 필요작업을 하게한다.
	// 해당 페이지를 전혀 접근하지 않는다면 이 모든 작업을 피할 수 있으며, 이것이 장점
	bool access;
	uint64_t *pml4;
	struct list page_list;
	struct list swap_list;
	uint64_t stack_bottom;
};

struct page_table_node {
	struct page *page;
	enum page_position;
	struct list_elem elem;
};

#include "threads/thread.h"
void supplemental_page_table_init (struct supplemental_page_table *spt);
bool supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src);
void supplemental_page_table_kill (struct supplemental_page_table *spt);
struct page *spt_find_page (struct supplemental_page_table *spt,
		void *va);
bool spt_insert_page (struct supplemental_page_table *spt, struct page *page, struct list * list);
void spt_remove_page (struct supplemental_page_table *spt, struct page *page, struct list * list);

void vm_init (void);
bool vm_try_handle_fault (struct intr_frame *f, void *addr, bool user,
		bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer ((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage,
		bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page (struct page *page);
bool vm_claim_page (void *va);
enum vm_type page_get_type (struct page *page);

size_t find_empty_disk_sector();
void restore_bitmap(size_t index);

void bitmap_lock_aquire();
void bitmap_lock_release();
#endif  /* VM_VM_H */
