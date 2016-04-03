#include "./include/irq.h"
#include "./include/x86/x86.h"
#include "./include/device/video.h"
#include "./include/game.h"
#include <sys/syscall.h>

static inline uint8_t sysin_byte(uint16_t port);
static inline void sysout_byte(uint16_t port,int8_t data);


void do_syscall(struct TrapFrame *tf){	
	switch(tf->ebx){
		case 0x100: //serial in	
			tf->eax = sysin_byte(tf->edx);
			break;
		case 0x101: //serial out
			sysout_byte(tf->edx,tf->eax);
			break;
		case 0x102: //display	
			display_buffer();	
			break;
		case 0x103: //halt
			printk("halt\n");
			while(1);break;
		case 0x104:
			tf->eax = query_key(tf->edx);break;
		case 0x105:
			tf->eax = query_direkey(tf->edx);break;
		case 0x106:
			tf->eax = query_blank();break;
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


