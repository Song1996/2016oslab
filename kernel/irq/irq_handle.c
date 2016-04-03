#include "include/x86/x86.h"
#include "include/game.h"
#define NR_IRQ_HANDLER 32

static void (*do_timer)(void);
static void (*do_keyboard)(int);
void (*handler_pool[NR_IRQ_HANDLER])(void);

void add_irq_handler(int irq,void(*func)(void)){
	handler_pool[irq] = func; 
}


void
set_timer_intr_handler( void (*ptr)(void) ) {
	do_timer = ptr;
}
void
set_keyboard_intr_handler( void (*ptr)(int) ) {
	do_keyboard = ptr;
}

void do_syscall(struct TrapFrame*);
static inline uint8_t sysin_byte(uint16_t port);
static inline void sysout_byte(uint16_t port,int8_t data);

/* TrapFrame的定义在include/x86/memory.h
 * 请仔细理解这段程序的含义，这些内容将在后续的实验中被反复使用。 */
void
irq_handle(struct TrapFrame *tf) {
	if(tf->irq<0){
		printk("tf->irq<0   %d\n",tf->irq);
		//assert(0);
	}else if(tf->irq==0x80){
		do_syscall(tf);
	}else if(tf->irq < 1000) {
			switch(tf->irq){
			case 6:
				printk("irq 6\n");
				break;
			case 14:
				printk("irq 14\n");
				break;
			case 13:
				printk("irq 13\n");
				printk("%x %x %x %x",tf->eax,tf->ebx,tf->ecx,tf->ecx);
				//assert(0);
				break;
			default:
				printk("unhandled exception!   %d\n", tf->irq);
				assert(0);
		}
	

	} else if (tf->irq == 1000) {	
		handler_pool[0]();
		//do_timer();
	} else if (tf->irq == 1001) {
		uint32_t code = in_byte(0x60);
		uint32_t val = in_byte(0x61);
		out_byte(0x61, val | 0x80);
		out_byte(0x61, val);
		printk("%s, %d: key code = %x\n", __FUNCTION__, __LINE__, code);
		handler_pool[1]();
		//do_keyboard(code);
	} else {
		assert(0);
	}
}

static inline uint8_t sysin_byte(uint16_t port){
	uint8_t data;
	asm volatile("in %1,%0": "=a"(data) : "d"(port));
	return data;
}

static inline void sysout_byte(uint16_t port,int8_t data){
	asm volatile("out %%al,%%dx" : : "a"(data), "d"(port));
}

