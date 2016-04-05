#include "./include/game.h"
#include "./include/irq.h"
#include "./include/x86/x86.h"
#include "./include/device/timer.h"
#include "./include/device/palette.h"
#include "./include/nassert.h"
#include "./boot/boot.h"
#include "./include/nstring.h"


#define SECTSIZE 512
#define ELF_PROG_LOAD 1

void printk_test(void);
uint32_t loader();
void readseg(unsigned char* , int, int);

typedef uint32_t* pde_t;
typedef uint32_t* pte_t;
extern pde_t entry_pgdir[];
extern pte_t entry_pgtable[];
extern void* pageinsert(pde_t*pgdir,const void* va,int create);
extern pde_t *kern_pgdir;
void env_init(void);
void mem_init(void);
void region_alloc(pde_t*,void*,size_t);
int env_alloc(void* newenv_store, int32_t parent_id);
pde_t* enter_snake();

static inline int 
in_long(short port) {
	int data;
	asm volatile("in %1, %0" : "=a" (data) : "d" (port));
	return data;
}

static inline int 
inbyte(short port){
	char data;
	asm volatile("in %1,%0" : "=a"(data) : "d"(port));
	return data;
}

static inline void
outbyte(short port,char data){
	asm volatile("out %0,%1" : : "a"(data),"d"(port));
}

void game_init(void) {	
	init_idt();	
	init_serial();
	init_timer();
	init_intr();	
	printk("entrypgdir %x\n",entry_pgdir);
	mem_init();
	env_init();
	pde_t* pgdir = 	enter_snake();
	/*printk("***************************\n");
	printk("my_pgdir %x\n",pgdir);
	printk("kern_pgdir %x\n",tpgdir);*/
	uint32_t tpgdir = (uint32_t)pgdir - 0xc0000000;
	asm volatile("movl %0,%%cr3" : : "r"(tpgdir));	
	printk("hello new pgdir\n");
	uint32_t eip = loader(tpgdir);
	printk("loader complete\n");
	printk("eip	%x\n",eip);
	printk("kernpgdir %x\n",kern_pgdir);
	((void(*)(void))eip)();
	printk("loader fail\n");
	while(1);
	assert(0); /* main_loop是死循环，永远无法返回这里 */
}

uint32_t loader(pde_t* pgdir){
	struct ELFHeader *elf;
	struct ProgramHeader *ph;
	uint8_t buf[4096];
	elf=(struct ELFHeader*)buf;
	readseg((unsigned char*)buf,4096,102400);
	const unsigned elf_magic = 0x464c457f;
	assert(*(unsigned*)elf == elf_magic);
	printk("find elf!\n");
	int j=0;
	for(;j<elf->phnum;j++){	
		ph=(void*)buf+elf->phoff+j*elf->phentsize;
		if(ph->type==ELF_PROG_LOAD){	
			region_alloc(pgdir,(void*)ph->vaddr,ph->memsz);		
			readseg((unsigned char*)ph->vaddr,ph->filesz,102400+ph->off);
			memset((void*)(ph->vaddr+ph->filesz),0,ph->memsz-ph->filesz);
		}
	}
	
	region_alloc(pgdir,(void*)(0x8000000-4096),4096);

	volatile uint32_t entry = elf->entry;	
	return entry;
	//if(*(uint32_t *)elf==elf_magic)while(1);	
	//return 0;
}


void
waitdisk(void) {
	while((inbyte(0x1F7) & 0xC0) != 0x40); /* 等待磁盘完毕 */
}







/* 读磁盘的一个扇区 */
void
readsect(void *dst, int offset) {
	int i;
	waitdisk();
	outbyte(0x1F2, 1);
	outbyte(0x1F3, offset);
	outbyte(0x1F4, offset >> 8);
	outbyte(0x1F5, offset >> 16);
	outbyte(0x1F6, (offset >> 24) | 0xE0);
	outbyte(0x1F7, 0x20);

	waitdisk();
	for (i = 0; i < SECTSIZE / 4; i ++) {
		((int *)dst)[i] = in_long(0x1F0);
	}
}

/* 将位于磁盘offset位置的count字节数据读入物理地址pa */
void
readseg(unsigned char *pa, int count, int offset) {
	unsigned char *epa;
	epa = pa + count;
	pa -= offset % SECTSIZE;
	offset = (offset / SECTSIZE) + 1;	
	for(; pa < epa; pa += SECTSIZE, offset ++){
		//printk("%x\n",pa);
		readsect(pa, offset);
	}
}


void printk_test(void){
	printk("game start!\n");
	printk("Printk test begin...\n");
	printk("the answer should be:\n");
	printk("#######################################################\n");
	printk("Hello, welcome to OSlab! I'm the body of the game. ");
	printk("Bootblock loads me to the memory position of 0x100000, and Makefile also tells me that I'm at the location of 0x100000. ");
	printk("~!@#$^&*()_+`1234567890-=...... ");
	printk("Now I will test your printk: ");
	printk("1 + 1 = 2, 123 * 456 = 56088\n0, -1, -2147483648, -1412505855, -32768, 102030\n0, ffffffff, 80000000, abcdef01, ffff8000, 18e8e\n");
	printk("#######################################################\n");
	printk("your answer:\n");
	printk("=======================================================\n");
	printk("%s %s%scome %co%s", "Hello,", "", "wel", 't', " ");
	printk("%c%c%c%c%c! ", 'O', 'S', 'l', 'a', 'b');
	printk("I'm the %s of %s. %s 0x%x, %s 0x%x. ", "body", "the game", "Bootblock loads me to the memory position of", 
				0x100000, "and Makefile also tells me that I'm at the location of", 0x100000);
	printk("~!@#$^&*()_+`1234567890-=...... ");
	printk("Now I will test your printk: ");
	printk("%d + %d = %d, %d * %d = %d\n", 1, 1, 1 + 1, 123, 456, 123 * 456);
	printk("%d, %d, %d, %d, %d, %d\n", 0, 0xffffffff, 0x80000000, 0xabcedf01, -32768, 102030);
	printk("%x, %x, %x, %x, %x, %x\n", 0, 0xffffffff, 0x80000000, 0xabcedf01, -32768, 102030);
	printk("=======================================================\n");
	printk("Test end!!! Good luck!!!\n");
}
