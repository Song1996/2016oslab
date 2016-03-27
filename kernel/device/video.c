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
#define SLOW

#ifdef TOOSLOW
	#define PARTIAL_UPDATE
	#define INTERLACE
#else
#ifdef SLOW
	#define PARTIAL_UPDATE
#endif
#endif

uint8_t *vmem = VMEM_ADDR;
static uint8_t vbuf[SCR_SIZE];
#ifdef PARTIAL_UPDATE
static uint8_t vref[SCR_SIZE];
#endif

void
prepare_buffer(void) {
#ifdef PARTIAL_UPDATE
	memcpy(vref, vbuf, SCR_SIZE);
#endif
	vmem = vbuf;
	memset(vmem, 0, SCR_SIZE);
}

void
display_buffer(void) {
#ifdef PARTIAL_UPDATE
	int i;
#ifdef INTERLACE
	static int update_line = 0;
	update_line ^= 1;
#endif
	uint32_t *buf = (uint32_t*)vbuf;
	uint32_t *ref = (uint32_t*)vref;
	uint32_t *mem = (uint32_t*)VMEM_ADDR;
	vmem = VMEM_ADDR;	
	for (i = 0; i < SCR_SIZE / 4; i ++) {
		if(i>=SCR_SIZE/4)break;
#ifdef INTERLACE
		if ((i * 4 / SCR_WIDTH) % 2 == update_line) {
			buf[i] = ref[i];
			continue;
		}
#endif
		if (buf[i] != ref[i]) {
			mem[i] = buf[i];
		}
	}
#else
	/*assert(0);
	vmem = VMEM_ADDR;
	uint32_t sysnum = 0x102;
	asm volatile ("int $0x80" : :"b"(sysnum),"c"(SCR_SIZE/4),"S"(vbuf),"D"(vmem));
	//asm volatile ("cld; rep movsl" : : "c"(SCR_SIZE / 4), "S"(vbuf), "D"(vmem));*/
#endif
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
	int i,j;
	for(i=0;i<150;i++)
		for(j=0;j<200;j++)
			draw_pixel(25+i,60+j,logo[i*200+j]);
}

