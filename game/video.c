#include "./include/common.h"
#include "./include/nstring.h"
#include "./include/device/video.h"
#include "./include/device/font.h"
#include "./include/device/logo.h"

/* 绘制屏幕的帧缓冲实现。
 * 在某些版本的qemu-kvm上，由于每次访问显存映射区域都会产生一次VM exit，
 * 更新屏幕的速度可能非常缓慢从而引起游戏跳帧。定义宏SLOW以只重绘屏幕变化
 * 的部分；定义宏TOOSLOW在只重绘屏幕变化部分的基础上，隔行更新。
 * TOOSLOW可能会引起视觉效果的损失。 */


uint8_t *vmem = VMEM_ADDR;
static uint8_t vbuf[SCR_SIZE];

static inline void
draw_pixel(int x, int y, int color) {
	assert(x >= 0 && y >= 0 && x < SCR_HEIGHT && y < SCR_WIDTH);
	vbuf[(x << 8) + (x << 6) + y] = color;
}


void
prepare_buffer(void) {
	vmem = vbuf;
	memset(vmem, 0, SCR_SIZE);
}

void
display_buffer(void) {
	asm volatile("int $0x80" : :"b"(0x102),"c"(vbuf));
}

void
draw_block(int x,int y,int color) {
	int i,j;
	for (i = 0; i < 19; i ++) 
		for (j = 0; j < 19; j ++) 
			draw_pixel( 10+x*20+i, 10+y*20+j, color);	
}


static inline void
draw_character(char ch, int x, int y, int color) {
	int i, j;
	assert((ch & 0x80) == 0);
	char *p = font8x8_basic[(int)ch];
	for (i = 0; i < 8; i ++) 
		for (j = 0; j < 8; j ++) 
			if ((p[i] >> j) & 1)
				draw_pixel(x + i, y + j, color);
}

void
draw_string(const char *str, int x, int y, int color) {
	while (*str) {
		draw_character(*str ++, x, y, color);
		if (y + 8 >= SCR_WIDTH) {
			x += 8; y = 0;
		} else {
			y += 8;
		}
	}
}

void
draw_logo(){
	printk("draw_logo\n");
	int i,j;
	for(i=0;i<150;i++)
		for(j=0;j<200;j++){
			draw_pixel(25+i,60+j,logo[i*200+j]);	
		}
}

