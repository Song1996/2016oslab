#include "./include/irq.h"
#include "./include/x86/x86.h"
#include "./include/device/video.h"
#include "./include/game.h"
#include <sys/syscall.h>

static inline uint8_t sysin_byte(uint16_t port);
static inline void sysout_byte(uint16_t port,int8_t data);
void add_irq_handler(int irq,void(*func)(void));
typedef void(*handler)(void);

void do_syscall(struct TrapFrame *tf){	
	switch(tf->ebx){
		case 0x0:
			disable_interrupt();
			//printk("we can add irq here\n");
			add_irq_handler(tf->ecx,(handler)tf->edx);
			enable_interrupt();
			break;
		case 0x100: //serial in	
			tf->eax = sysin_byte(tf->edx);
			break;
		case 0x101: //serial out
			sysout_byte(tf->edx,tf->eax);
			break;
		case 0x102:	
			for(int i=0;i<320*200;i++)
			  ((char*)0xa0000)[i]=((char*)tf->ecx)[i];
			break;
		case 0x103: //halt
			printk("halt\n");
			while(1);break;
		default:
			printk("unhandled system call : id = %d",tf->eax);
			assert(0);
	}

}

static inline uint8_t sysin_byte(uint16_t port){
	uint8_t data;
	asm volatile("in %1,%0":"=a"(data):"d"(port));
	return data;
}

static inline void sysout_byte(uint16_t port,int8_t data){
	asm volatile("out %%al,%%dx": : "a"(data),"d"(port));
}


