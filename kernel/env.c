#include <./include/jos/x86.h>
#include <./include/jos/mmu.h>
#include <./include/jos/error.h>
#include <./include/jos/string.h>
#include <./include/jos/assert.h>
#include <./include/jos/elf.h>
#include <./include/jos/env.h>

//#include <kern/env.h>
//#include <kern/pmap.h>
//#include <kern/trap.h>
//#include <kern/monitor.h>

extern void printk(const char*,...);
extern void env_init_percpu(void);
struct Env* envs;
static struct Env *env_free_list;

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
	//while(1);
	printk("envs start%x\n",envs);
	printk("envs end%x\n",&envs[NENV-1]);
	env_free_list = NULL;
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
