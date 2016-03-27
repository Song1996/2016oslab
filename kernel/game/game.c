#include "./include/x86/x86.h"
#include "./include/game.h"
#include "./include/nstring.h"
#include "./include/device/timer.h"

#define UPDATERATE 3
#define FPS 30
#define CHARACTER_PER_SECOND 5
#define UPDATE_PER_SECOND 100

#define FOOD_PER_TENSECOND 5


volatile int tick = 0;

void
timer_event(void) {
	tick ++;
}

static int real_fps;
void
set_fps(int value) {
	real_fps = value;
}
int
get_fps() {
	return real_fps;
}

/* 游戏主循环。
 * 在初始化工作结束后，main函数就跳转到主循环执行。
 * 在主循环执行期间随时会插入异步的中断。时钟中断最终调用timer_event，
 * 键盘中断最终调用keyboard_event。中断处理完成后将返回主循环原位置继续执行。
 *
 * tick是时钟中断中维护的信号，数值含义是“系统到当前时刻已经发生过的时钟中断数”
 * HZ是时钟控制器硬件每秒产生的中断数，在include/device/timer.h中定义
 * now是主循环已经正确处理的时钟中断数，即游戏已经处理到的物理时间点
 *
 * 由于qemu-kvm在访问内存映射IO区域时每次都会产生陷入，在30FPS时，
 * 对显存区域每秒会产生30*320*200/4次陷入，从而消耗过多时间导致跳帧的产生(实际FPS<30)。
 * 在CFLAGS中增加-DSLOW可以在此情况下提升FPS。如果FPS仍太小，可以尝试
 * -DTOOSLOW，此时将会采用隔行扫描的方式更新屏幕(可能会降低显示效果)。
 * 这些机制的实现在device/video.c中。
 * */



static bool querysys_blank(){
	bool flag;
	asm volatile("int $0x80" : "=a"(flag) : "b"(0x106));
	return flag;
}

void
main_loop(void) {
	int now = 0, target;
	int num_draw = 0;
	bool redraw;	
	while(update_keypress());	
	show_logo();
	while(!querysys_blank());
	now=tick;
	while (TRUE) {
		wait_for_interrupt();
		disable_interrupt();
		if (now == tick) {
			enable_interrupt();
			continue;
		}
		assert(now < tick);
		target = tick; /* now总是小于tick，因此我们需要“追赶”当前的时间 */
		enable_interrupt();
		redraw = FALSE;
		while (update_keypress());
		while (now < target) { 
			srand(tick);
			if (now % (HZ / UPDATERATE)==0)update_snake_pos();
			if (now % (HZ / FPS) == 0) {
				redraw = TRUE;
			}
			/* 更新fps统计信息 */
			if (now % (HZ / 2) == 0) {
				int now_fps = num_draw * 2 + 1;
				if (now_fps > FPS) now_fps = FPS;
				set_fps(now_fps);
				num_draw = 0;
			}
			now ++;
		}
		if(get_ggflag())break;
		if (redraw) { /* 当需要重新绘图时重绘 */
			num_draw ++;
			redraw_screen();
		}
	}
	draw_gg();
}
