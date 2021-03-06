#include "include/common.h"
#include "include/nstring.h"

static inline uint8_t
in_byte(uint16_t port){
	uint8_t data;
	asm volatile ("in %1,%0" : "=a"(data) : "d"(port));
	return data;
}

static inline void
out_byte(uint16_t port,int8_t data){
	asm volatile("out %%al,%%dx" : : "a"(data),"d"(port));
}


/* a-z对应的键盘扫描码 */
static int letter_code[] = {
	30, 48, 46, 32, 18, 33, 34, 35, 23, 36,
	37, 38, 50, 49, 24, 25, 16, 19, 31, 20,
	22, 47, 17, 45, 21, 44, 
	75,/*LEFT*/ 77,/*RIGHT*/ 72,/*UP*/ 80,/*DOWN*/
	57,/*SPACE*/
};


/* 对应键按下的标志位 */
static bool letter_pressed[26+4+1];

void
press_key(int scan_code) {
	int i;
	for (i = 0; i < (26+4+1); i ++) {
		if (letter_code[i] == scan_code) {
			letter_pressed[i] = TRUE;
		}
	}
}

void
release_key(int index) {
	assert(0 <= index && index < 26+4+1);
	letter_pressed[index] = FALSE;
}

bool query_blank(void){
	if(letter_pressed[26+4]){
		release_key(30);
		return TRUE;
	}
	else 
		return FALSE;
}

bool
query_key(int index) {
	assert(0 <= index && index < 26);
	return letter_pressed[index];
}

bool 
query_direkey(int index) {
	assert(0 <= index && index <4);
	return letter_pressed[index+26];
}


/* key_code保存了上一次键盘事件中的扫描码 */
static volatile int key_code = 0;

int last_key_code(void) {
	return key_code;
}

void
keyboard_event() {
	uint32_t code = in_byte(0x60);
	uint32_t val = in_byte(0x61);
	out_byte(0x61,val | 0x80);
	out_byte(0x61,val);
	//printk("%s,%d: key code =%x\n",__FUNCTION__,__LINE__,code);
	key_code = code;
	press_key(code);
}

