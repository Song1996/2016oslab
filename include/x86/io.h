#ifndef __X86_IO_H__
#define __X86_IO_H__

#include "./include/game.h"

/* 读I/O端口 */
static inline uint8_t
in_byte(uint16_t port) {
	uint8_t data;	
	uint32_t sysnum = 0x100;
	asm volatile("in %1, %0" : "=a"(data) : "d"(port));
	//asm volatile("int  $0x80" : "=a"(data):"b"(sysnum), "d"(port));
	return data;
}

/* 写I/O端口 */
static inline void
out_byte(uint16_t port, int8_t data) {
	uint32_t sysnum=0x101;
	asm volatile("out %%al, %%dx" : : "a"(data), "d"(port));
	//asm volatile("int  $0x80" : : "a"(data),"b" (sysnum),"d"(port));
}

#endif
