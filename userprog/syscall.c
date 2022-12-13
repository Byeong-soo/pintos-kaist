#include "userprog/syscall.h"

#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

#include "filesys/filesys.h"
#include "lib/user/syscall.h"
#include "../include/filesys/file.h"
#include "filesys/inode.h"
#include "../include/userprog/process.h"

#include "userprog/process.h"
#include "threads/mmu.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual.
 *
 * x86-64에서는 syscall 으로 시스템 콜을 요청하는 방법이 제공되고, syscall 인스트럭션은 MSR로 부터 값을 읽어옴으로써 동작한다 */

#define MSR_STAR 0xc0000081			/* Segment selector msr */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void)
{
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	lock_init(&filesys_lock);
}

/* The main system call interface */

// syscall function

void syscall_halt(void);
void syscall_exit(struct intr_frame *f);
// fork func parameter : const char *thread_name
pid_t syscall_fork(struct intr_frame *f);
// exec func parameter : const char *cmd_line
// int syscall_exec (const char *cmd_line);
int syscall_exec(struct intr_frame *f);
// wait func parameter : pid_t pid
int syscall_wait(struct intr_frame *f);
bool syscall_create(struct intr_frame *f);
// remove func parameter : chonst char *file
bool syscall_remove(struct intr_frame *f);
// open func parameter : const char *file
int syscall_open(struct intr_frame *f);
// filesize func parameter : int fd
int syscall_filesize(struct intr_frame *f);
// read func parameter : int fd, void *buffer, unsigned size
int syscall_read(struct intr_frame *f);
// write func parameter : int fd, const void *buffer, unsigned size
void syscall_write(struct intr_frame *f);
// seek func parameter : int fd, unsigned position
void syscall_seek(struct intr_frame *f);
// tell func parameter : int fd
unsigned syscall_tell(struct intr_frame *f);
// close func larameter : int fd
void syscall_close(struct intr_frame *f);

void * syscall_mmap(struct intr_frame *f);

void syscall_munmap(struct intr_frame *f);

// 공용 함수

void syscall_abnormal_exit(short exit_code);
// f의 값 출력 type 0 : d 출력, 1 : s 출력, 2 : 둘다 출력
void print_values(struct intr_frame *f, int type);

bool check_ptr_address(struct intr_frame *f);

struct fd *find_matched_fd(int fd_value);

struct syscall_func syscall_func[] = {
	{SYS_HALT, syscall_halt},
	{SYS_EXIT, syscall_exit},
	{SYS_FORK, syscall_fork},
	{SYS_EXEC, syscall_exec},
	{SYS_WAIT, syscall_wait},
	{SYS_CREATE, syscall_create},
	{SYS_REMOVE, syscall_remove},
	{SYS_OPEN, syscall_open},
	{SYS_FILESIZE, syscall_filesize},
	{SYS_READ, syscall_read},
	{SYS_WRITE, syscall_write},
	{SYS_SEEK, syscall_seek},
	{SYS_TELL, syscall_tell},
	{SYS_CLOSE, syscall_close},
	{SYS_MMAP, syscall_mmap},
	{SYS_MUNMAP, syscall_munmap},
};

/* The main system call interface */
void syscall_handler(struct intr_frame *f)
{
	// !전체 시스템 콜에 영향을 주는 곳입니다. 최대한 작성을 지양 해주세요
	struct syscall_func call = syscall_func[f->R.rax];
	call.function(f);
}

// syscall function

void syscall_halt(void)
{
	power_off();
}

void syscall_exit(struct intr_frame *f)
{
	thread_current()->exit_code = f->R.rdi;
	thread_exit();
}

// fork func parameter : const char *thread_name
pid_t syscall_fork(struct intr_frame *f)
{
	char *thread_name = f->R.rdi;
	int return_value;
	// printf("welecome in fork\n");
	return_value = process_fork(thread_name, f);
	// printf("fork return_value %d\n",return_value);
	f->R.rax = return_value;
	return return_value;
}

// exec func parameter : const char *cmd_line
// int syscall_exec (const char *cmd_line){
int syscall_exec(struct intr_frame *f)
{
	char *file_name = f->R.rdi;
	char *fn_copy;

	check_addr(file_name);
	fn_copy = palloc_get_page(0);
	if (fn_copy == NULL)
	{
		syscall_abnormal_exit(EXIT_CODE_ERROR);
		palloc_free_page(fn_copy);
		f->R.rax = -1;
		return -1;
	}
	strlcpy(fn_copy, file_name, PGSIZE); // filename을 fn_copy로 복사
	if (process_exec(fn_copy) < 0)
	{
		palloc_free_page(fn_copy);
		f->R.rax = -1;
		syscall_abnormal_exit(-1);
	}
}

// wait func parameter : pid_t pid
int syscall_wait(struct intr_frame *f)
{
	int pid = f->R.rdi;
	struct child_info *child_info = search_children_list(pid);

	if (child_info == NULL)
	{
		f->R.rax = -1;
		return -1;
	}

	int return_value;

	if (child_info->exit_code == EXIT_CODE_DEFAULT)
	{
		return_value = process_wait(pid);
		f->R.rax = return_value;
	}
	else
	{
		return_value = child_info->exit_code;
		f->R.rax = return_value;
	}

	list_remove(&child_info->elem);
	free(child_info);
	// printf("end_wait\n");
	return return_value;
}

bool syscall_create(struct intr_frame *f)
{
	bool success;
	check_addr(f->R.rdi);

	if (f->R.rdi == NULL)
	{
		syscall_abnormal_exit(-1);
	}

	lock_acquire(&filesys_lock);
	success = filesys_create(f->R.rdi, f->R.rsi);
	lock_release(&filesys_lock);
	f->R.rax = success;
	return success;
}

// remove func parameter : chonst char *file
bool syscall_remove(struct intr_frame *f)
{
	bool success;
	char *file = f->R.rdi;

	check_addr(file);

	lock_acquire(&filesys_lock);
	success = filesys_remove(file);
	lock_release(&filesys_lock);

	f->R.rax = success;
	return success;
}

// open func parameter : const char *file
int syscall_open(struct intr_frame *f)
{	
	// printf("ptr %X \n",f->R.rdi);
	check_addr(f->R.rdi);
	struct list *fd_list;

	if(f->R.rdi == NULL){
		f->R.rax = -1;
		return;
	}
	// if (thread_current()->fd_count > 20)
	// {
	// 	f->R.rax = -1;
	// 	return -1;
	// }

	fd_list = &thread_current()->fd_list;

	lock_acquire(&filesys_lock);
	struct file *open_file = filesys_open(f->R.rdi);
	lock_release(&filesys_lock);

	if (open_file == NULL)
	{
		f->R.rax = -1;
		return -1;
	}

	struct fd *fd = (struct fd *)malloc(sizeof(struct fd));

	fd->value = thread_current()->fd_count + 1;
	fd->file = open_file;

	list_push_front(fd_list, &fd->elem);
	thread_current()->fd_count += 1;

	f->R.rax = fd->value;
	return fd->value;
}

// filesize func parameter : int fd
int syscall_filesize(struct intr_frame *f)
{
	int fd_value = f->R.rdi;
	struct fd *find_fd = find_matched_fd(fd_value);

	if (find_fd == NULL)
	{
		f->R.rax = -1;
		return -1;
	}
	int size = file_length(find_fd->file); 
	f->R.rax = size;
	return size;
}

// read func parameter : int fd, void *buffer, unsigned size
int syscall_read(struct intr_frame *f)
{	
	struct thread* t = thread_current();
	struct page * start_page,*end_page;
	// print_values(f,0);
	// printf("is user = %d\n",is_user_vaddr(pg_round_down(f->R.rsi)));
	// printf("is user = %d\n",USER_STACK > f->R.rsi);
	// printf("buffer %X\n",f->R.rsi);
	// printf("stack bottom = %X\n",thread_current()->spt.stack_bottom);
	// printf("kern_base = %X\n",KERN_BASE);
	// printf("USER_STACK = %X\n",USER_STACK);
	// printf("testing %X\n",testing);

	start_page = spt_find_page(&t->spt,pg_round_down(f->R.rsi));
	end_page = spt_find_page(&t->spt,pg_round_down(f->R.rsi + f->R.rdx -1));
	// if (is_kernel_vaddr((f->R.rsi)) || pml4_get_page(t->pml4, f->R.rsi) == NULL)

	// printf("int fd = %d\n",f->R.rdi);
	// printf("read rsi = %X\n",f->R.rsi);
	// printf("stack bottom = %X\n",t->spt.stack_bottom);
	// printf("page va = %X\n",page->va);
	// printf("page writable = %d\n",page->writeable);
	// if(page == NULL || page->writeable == 0){
	if((start_page == NULL || end_page == NULL )|| start_page->writable == 0){
		syscall_abnormal_exit(-1);
	}

	// if((f->R.rsi < t->spt.stack_bottom || f->R.rsi > USER_STACK)){
	// 	syscall_abnormal_exit(-1);
	// }

	// if (!(is_user_vaddr(pg_round_down(f->R.rsi)) && USER_STACK > f->R.rsi))
	// {
	// 	syscall_abnormal_exit(-1);
	// }

	// check_addr(f->R.rsi);
	int fd_value, size;
	fd_value = f->R.rdi;
	char *buf = f->R.rsi;
	size = f->R.rdx;

	int return_value;
	struct fd *read_fd = find_matched_fd(fd_value);

	if (read_fd == NULL)
	{
		f->R.rax = -1;
		return -1;
	}

	lock_acquire(&filesys_lock);
	return_value = file_read(read_fd->file, buf, size);
	lock_release(&filesys_lock);
	f->R.rax = return_value;
	// printf("file pos %d\n",read_fd->file->pos);
	// printf("read fd value = %d, ofrigin offset = %d file length %X\n",read_fd->value,read_fd->file->pos,file_length(read_fd->file));
	// hex_dump(buf,buf,4096,true);
	// printf("end_syscall_read\n");
	return return_value;
}

// write func parameter : int fd, const void *buffer, unsigned size
void syscall_write(struct intr_frame *f)
{
	struct thread * t = thread_current();
	struct page * start_page, *end_page;
	// if (!(is_user_vaddr(pg_round_down(f->R.rsi)) && USER_STACK > f->R.rsi))
	// {
	// 	syscall_abnormal_exit(-1);
	// }
	// printf("\nwrite rsi = %d\n\n",f->R.rsi);
	start_page = spt_find_page(&t->spt,pg_round_down(f->R.rsi));
	end_page = spt_find_page(&t->spt,pg_round_down(f->R.rsi + f->R.rdx -1));

	// printf()


	// if( page == NULL || page->writeable == 1){
	// if((start_page == NULL || end_page == NULL ) || start_page->writable == 0){
	if((start_page == NULL || end_page == NULL )){
		syscall_abnormal_exit(-1);
	}


	// if(f->R.rsi < t->spt.stack_bottom || f->R.rsi > USER_STACK){
	// 	syscall_abnormal_exit(-1);
	// }

	if (!(is_user_vaddr(pg_round_down(f->R.rsi)) && USER_STACK > f->R.rsi))
	{
		syscall_abnormal_exit(-1);
	}
	// check_addr(f->R.rsi);
	int fd_value = f->R.rdi;
	char *buf = f->R.rsi;
	int size = f->R.rdx;
	if (fd_value == 1)
	{
		putbuf(buf, size);
		return;
	}

	int return_value;
	struct fd * write_fd = find_matched_fd(fd_value);

	if (write_fd == NULL || write_fd->file->deny_write)
	{
		f->R.rax = -1;
		return -1;
	}

	// printf("write fd value = %d\n",write_fd->value);
	// printf("write buffer = %s\n",buf);
	// printf("write size = %d\n",size);

	lock_acquire(&filesys_lock);
	return_value = file_write(write_fd->file, buf, size);
	lock_release(&filesys_lock);


	// printf("hi iam bye!!\n");

	f->R.rax = return_value;
	return return_value;
}

// seek func parameter : int fd, unsigned position
void syscall_seek(struct intr_frame *f)
{
	int fd_value = f->R.rdi;
	unsigned int offset = f->R.rsi;

	struct list *fd_list = &thread_current()->fd_list;
	struct fd *find_fd;

	find_fd = find_matched_fd(fd_value);
	if (find_fd == NULL)
	{
		syscall_abnormal_exit(-1);
	}
	file_seek(find_fd->file, offset);
}

// tell func parameter : int fd
unsigned syscall_tell(struct intr_frame *f)
{
	int fd_value = f->R.rdi;
	struct list *fd_list = &thread_current()->fd_list;
	struct fd *find_fd;

	find_fd = find_matched_fd(fd_value);
	if (find_fd == NULL)
	{
		syscall_abnormal_exit(-1);
	}

	unsigned int position = file_tell(find_fd->file);
	f->R.rax = position;
	return position;
}

// close func larameter : int fd
void syscall_close(struct intr_frame *f)
{
	int fd_value = f->R.rdi;
	struct list *fd_list = &thread_current()->fd_list;
	struct fd *find_fd;

	find_fd = find_matched_fd(fd_value);

	if (find_fd == NULL)
	{	
		syscall_abnormal_exit(-1);
	}

	lock_acquire(&filesys_lock);
	file_close(find_fd->file);
	list_remove(&find_fd->elem);
	free(find_fd);
	lock_release(&filesys_lock);
}
// mmap (void *addr, size_t length, int writable, int fd, off_t offset)
void * syscall_mmap(struct intr_frame *f){
	void* addr = f->R.rdi;
	size_t filesize = f->R.rsi;
	int writable = f->R.rdx;
	int fd_value = f->R.r10;
	off_t offset = f->R.r8;
	// printf("enter mmap !\n");
	struct page * page;
	
	struct fd *read_fd = find_matched_fd(fd_value);
	page = spt_find_page(&thread_current()->spt,addr);
	// printf("after page!\n");
	if( read_fd == NULL || filesize == NULL || addr == NULL || pg_ofs(addr) != 0 
	|| page != NULL || pg_ofs(offset)){
		f->R.rax = 0;
		return  0;
	}

	off_t origin_offset = read_fd->file->pos;
	// printf("mmap_fd value = %d, ofrigin offset = %d file_length = %X\n",read_fd->value,read_fd->file->pos,file_length(read_fd->file));
	// printf("before enter mmap !\n");
	uint64_t return_value;
	return_value = do_mmap(addr,filesize,writable,read_fd->file,offset);
	f->R.rax = return_value;
	// printf("after enter mmap return value %X\n",return_value);

	// file_lock_acquire();
	// file_seek(read_fd->file,origin_offset);
	// file_lock_release();
	return return_value;
}

void syscall_munmap(struct intr_frame *f){
	void * mmap = f->R.rdi;
	
	// printf("start mm");
	do_munmap(mmap);
	// printf("end munmap");
}



// 공용 함수

void syscall_abnormal_exit(short exit_code)
{
	thread_current()->exit_code = exit_code;
	thread_exit();
}

// f의 값 출력 type 0 : d 출력, 1 : s 출력, 2 : 둘다 출력
void print_values(struct intr_frame *f, int type)
{
	printf("call_num   %d\n", f->R.rax);
	printf("rdi        %X\n", f->R.rdi);
	if (type == 0)
	{
		printf("rsi        %d\n", f->R.rsi);
	}
	else if (type == 1)
	{
		if (f->R.rsi == 0)
		{
			printf("rsi        %s\n", f->R.rsi);
		}
		else
		{
			printf("rsi        %s", f->R.rsi);
		}
	}
	else if (type == 2)
	{
		printf("rsi        %d\n", f->R.rsi);
		if (f->R.rsi == 0)
		{
			printf("rsi        %s\n", f->R.rsi);
		}
		else
		{
			printf("rsi        %s", f->R.rsi);
		}
	}
	printf("rdx        %d\n", f->R.rdx);
	printf("r10        %d\n", f->R.r10);
	printf("r8         %d\n", f->R.r8);
	printf("r9         %d\n", f->R.r9);
}

bool check_ptr_address(struct intr_frame *f)
{
	bool success = false;
	if (f->rsp < f->R.rdi && f->rsp + (1 << 12) < f->R.rdi)
	{
		success = true;
	}
	return success;
}

void check_addr(void *addr)
{	
	// printf("check addr enter\n");
	// printf("check addr = %X\n",addr);
	struct thread *t = thread_current();
	// if(pml4_get_page(t->pml4,addr) != NULL){
	// 	return;
	// }

	// printf("is_user_vaddr = %d\n",is_user_vaddr(pg_round_down(addr)));
	// printf("USERSTACK  = %d\n",USER_STACK > addr);

	if (is_kernel_vaddr((addr)) || pml4_get_page(t->pml4, addr) == NULL)
	// if (!(is_user_vaddr(pg_round_down(addr)) && USER_STACK > addr))
	{
		syscall_abnormal_exit(-1);
	}
}

struct fd *
find_matched_fd(int fd_value)
{
	struct list *fd_list = &thread_current()->fd_list;
	struct list_elem *cur;

	if (list_empty(fd_list))
	{
		return NULL;
	}

	cur = list_begin(fd_list);
	while (cur != list_end(fd_list))
	{
		struct fd *find_fd = list_entry(cur, struct fd, elem);
		if (find_fd->value == fd_value)
		{
			return find_fd;
			;
		}
		cur = list_next(cur);
	}
	return NULL;
}

void file_lock_acquire()
{
	lock_acquire(&filesys_lock);
}
void file_lock_release()
{
	lock_release(&filesys_lock);
}

bool check_file_lock_holder(){
	if(filesys_lock.holder == thread_current())
		return true;
	return false;
}