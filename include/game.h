#ifndef __GAME_H__
#define __GAME_H__

#include "./include/common.h"
//#include "./include/adt/linklist.h"

/* 初始化串口 */
void init_serial();

/* 中断时调用的函数 */
void timer_event(void);
void keyboard_event(int scan_code);

/* 按键相关 */
void press_key(int scan_code);
void release_key(int ch);
bool query_key(int ch);
bool query_blank(void);
bool query_direkey(int ch);
int last_key_code(void);

/* 定义链表 */




/* 主循环 */
void main_loop(void);

/* 游戏逻辑相关 */
void create_new_food(void);
void update_snake_pos(void);
bool update_keypress(void);

int get_len(void);
int get_fps(void);
bool get_ggflag(void);
void set_fps(int fps);


void redraw_screen(void);
void draw_gg(void);
void show_logo(void);

/* 随机数 */
int rand(void);
void srand(int seed);

#endif
