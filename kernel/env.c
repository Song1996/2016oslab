#include <./include/jos/x86.h>
#include <./include/jos/mmu.h>
#include <./include/jos/error.h>
#include <./include/jos/string.h>
#include <./include/jos/assert.h>
#include <./include/jos/elf.h>
#include <./include/jos/env.h>
#include <./include/jos/pmap.h>

#define ENVGENSHIFT 12
//#include <kern/env.h>
//#include <kern/pmap.h>
//#include <kern/trap.h>
//#include <kern/monitor.h>

extern void printk(const char*,...);
extern void env_init_percpu(void);
struct Env* envs;
static struct Env *env_free_list;
void region_alloc(pde_t*,void*,size_t);
struct PageInfo* page_alloc(int);

//
struct Segdesc gdt[] =
{
	// 0x0 - unused (always faults -- for trapping NULL far pointers)
	[0] = SEG_NULL ,

	// 0x8 - kernel code segment
	[GD_KT >> 3] = SEG(STA_X | STA_R, 0x0, 0xffffffff, 0),

	// 0x10 - kernel data segment
	[GD_KD >> 3] = SEG(STA_W, 0x0, 0xffffffff, 0),

	// 0x18 - user code segment
	[GD_UT >> 3] = SEG(STA_X | STA_R, 0x0, 0xffffffff, 3),

	// 0x20 - user data segment
	[GD_UD >> 3] = SEG(STA_W, 0x0, 0xffffffff, 3),

	// 0x28 - tss, initialized in trap_init_percpu()
	[GD_TSS0 >> 3] = SEG_NULL
};

struct Pseudodesc gdt_pd = {
	sizeof(gdt) - 1, (unsigned long) gdt
};

void
env_init(){	
	for(int i=NENV-1;i>=0;i--){	
		envs[i].env_id=0;
		envs[i].env_parent_id=0;
		envs[i].env_type=ENV_TYPE_IDLE;
		envs[i].env_status=0;
		envs[i].env_runs=0;
		envs[i].env_pgdir=NULL;
		envs[i].env_link = env_free_list;
		env_free_list = &envs[i];
	}
	printk("envs start%x\n",envs);
	printk("envs end%x\n",&envs[NENV-1]);
	env_init_percpu();
}


void
env_init_percpu(void)
{
	printk("loader new gdt\n");
	lgdt(&gdt_pd);
	// The kernel never uses GS or FS, so we leave those set to
	// the user data segment.
	asm volatile("movw %%ax,%%gs" :: "a" (GD_UD|3));
	asm volatile("movw %%ax,%%fs" :: "a" (GD_UD|3));
	// The kernel does use ES, DS, and SS.  We'll change between
	// the kernel and user data segments as needed.
	asm volatile("movw %%ax,%%es" :: "a" (GD_KD));
	asm volatile("movw %%ax,%%ds" :: "a" (GD_KD));
	asm volatile("movw %%ax,%%ss" :: "a" (GD_KD));
	// Load the kernel text segment into CS.
	asm volatile("ljmp %0,$1f\n 1:\n" :: "i" (GD_KT));
	// For good measure, clear the local descriptor table (LDT),
	// since we don't use it.
	lldt(0);
}

static int
env_setup_vm(struct Env *e)
{

	struct PageInfo *p = NULL;

	// Allocate a page for the page directory
	if (!(p = page_alloc(ALLOC_ZERO)))
		return -E_NO_MEM;

	// Now, set e->env_pgdir and initialize the page directory.
	//
	// Hint:
	//    - The VA space of all envs is identical above UTOP
	//	(except at UVPT, which we've set below).
	//	See inc/memlayout.h for permissions and layout.
	//	Can you use kern_pgdir as a template?  Hint: Yes.
	//	(Make sure you got the permissions right in Lab 2.)
	//    - The initial VA below UTOP is empty.
	//    - You do not need to make any more calls to page_alloc.
	//    - Note: In general, pp_ref is not maintained for
	//	physical pages mapped only above UTOP, but env_pgdir
	//	is an exception -- you need to increment env_pgdir's
	//	pp_ref for env_free to work correctly.
	//    - The functions in kern/pmap.h are handy.

	// LAB 3: Your code here.

	e->env_pgdir = page2kva(p);
	p->pp_ref++;
	for(int i=0;i<1024;i++)
	  e->env_pgdir[i] = kern_pgdir[i]; 

	// UVPT maps the env's own page table read-only.
	// Permissions: kernel R, user R
//	e->env_pgdir[PDX(UVPT)] = PADDR(e->env_pgdir) | PTE_P | PTE_U;

	return 0;
}

int
env_alloc(struct Env **newenv_store, envid_t parent_id)
{
	int32_t generation;
	int r;
	struct Env *e;
	if (!(e = env_free_list))
		return -E_NO_FREE_ENV;
	if ((r = env_setup_vm(e)) < 0)
		return r;
	generation = (e->env_id + (1 << ENVGENSHIFT)) & ~(NENV - 1);
	if (generation <= 0)	// Don't create a negative env_id.
		generation = 1 << ENVGENSHIFT;
	e->env_id = generation | (e - envs);
	e->env_parent_id = parent_id;
	e->env_type = ENV_TYPE_USER;
	e->env_status = ENV_RUNNABLE;
	e->env_runs = 0;
	memset(&e->env_tf, 0, sizeof(e->env_tf));

	e->env_tf.tf_ds = GD_UD | 3;
	e->env_tf.tf_es = GD_UD | 3;
	e->env_tf.tf_ss = GD_UD | 3;
	e->env_tf.tf_esp = 0x8000000;//USTACKTOP;
	e->env_tf.tf_cs = GD_UT | 3;
	// You will set e->env_tf.tf_eip later.

	// commit the allocation
	env_free_list = e->env_link;
	*newenv_store = e;

	//printk("[%08x] new env %08x\n", curenv ? curenv->env_id : 0, e->env_id);
	return 0;
}


uint32_t loader(pde_t* pgdir);
void region_alloc(pde_t*,void*,size_t);

void
env_pop_tf(struct Trapframe *tf)
{
	__asm __volatile("movl %0,%%esp\n"
		"\tpopal\n"
		"\tpopl %%es\n"
		"\tpopl %%ds\n"
		"\taddl $0x8,%%esp\n" /* skip tf_trapno and tf_errcode */
		"\tiret"
		: : "g" (tf) : "memory");
	//panic("iret failed");  /* mostly to placate the compiler */
	printk("iret fail\n");
	while(1);
}


pde_t* enter_snake(){
	struct Env* e;
	if(env_alloc(&e,0)!=0){
		printk("env_alloc fail\n");
		while(1);
	}
//return e->env_pgdir;
	lcr3((uint32_t)e->env_pgdir-0xc0000000);	
	e->env_tf.tf_eip = loader(e->env_pgdir);
	printk("enter snake :: loader complete\n");
	printk("eip %x\n",e->env_tf.tf_eip);
	//((void(*)(void))e->env_tf.tf_eip)();
	env_pop_tf(&(e->env_tf));
	return NULL;
}


/***************************************************************
static void
region_alloc(struct Env *e, void *va, size_t len)
{
	// LAB 3: Your code here.
	// (But only if you need it for load_icode.)
	//
	// Hint: It is easier to use region_alloc if the caller can pass
	//   'va' and 'len' values that are not page-aligned.
	//   You should round va down, and round (va + len) up.
	//   (Watch out for corner-cases!)
	void* i = (void*) ROUNDDOWN(va,PGSIZE);
	void* end = (void*) ROUNDUP(va+len,PGSIZE);
	while(i<end){
		struct PageInfo* page = page_alloc(0);
		page_insert(pgdir,page,begin,PTE_U|PTE_W);
		i+=PGSIZE;
	}	
}
static void
load_icode(struct Env *e, uint8_t *binary)
{
	// Hints:
	//  Load each program segment into virtual memory
	//  at the address specified in the ELF section header.
	//  You should only load segments with ph->p_type == ELF_PROG_LOAD.
	//  Each segment's virtual address can be found in ph->p_va
	//  and its size in memory can be found in ph->p_memsz.
	//  The ph->p_filesz bytes from the ELF binary, starting at
	//  'binary + ph->p_offset', should be copied to virtual address
	//  ph->p_va.  Any remaining memory bytes should be cleared to zero.
	//  (The ELF header should have ph->p_filesz <= ph->p_memsz.)
	//  Use functions from the previous lab to allocate and map pages.
	//
	//  All page protection bits should be user read/write for now.
	//  ELF segments are not necessarily page-aligned, but you can
	//  assume for this function that no two segments will touch
	//  the same virtual page.
	//
	//  You may find a function like region_alloc useful.
	//
	//  Loading the segments is much simpler if you can move data
	//  directly into the virtual addresses stored in the ELF binary.
	//  So which page directory should be in force during
	//  this function?
	//
	//  You must also do something with the program's entry point,
	//  to make sure that the environment starts executing there.
	//  What?  (See env_run() and env_pop_tf() below.)

	// LAB 3: Your code here.
	struct ELF *elf;
	struct Proghdr *ph;
	uint8_t buf[4096];
	elf = (struct ELF*)buf;
	readseg(())
	// Now map one page for the program's initial stack
	// at virtual address USTACKTOP - PGSIZE.

	// LAB 3: Your code here.
}
*/
