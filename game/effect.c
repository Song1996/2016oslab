#include "include/game.h"
#include "include/common.h"
#include "include/nstring.h"
#include "include/device/video.h"
#include "include/x86/x86.h"



#define COLNUM	15
#define ROWNUM	9
#define DIRENUM 4
#define DIREL   0
#define DIRER   1
#define DIREU	2
#define DIRED	3
#define FOOD	1
#define SNAKE	2
#define EMPTY	0
static int gamepool [ROWNUM][COLNUM];

struct snakenode {
	int x,y;
	int direction;
	bool used;
};

static struct snakenode snake[ COLNUM * ROWNUM ];
static int snakelen ;
static bool ggflag = FALSE;

int
get_len(void) {
	return snakelen;
}

bool
get_ggflag(void) {
	return ggflag;
}


/* 在屏幕上创建一个新的食物 */
void
create_new_food(void) {
	int y=rand()%ROWNUM;
	int x=rand()%COLNUM;
	while(gamepool[y][x]!=EMPTY){
		y=rand()%ROWNUM;
		x=rand()%COLNUM;
	}
	gamepool[y][x] = FOOD;
}

/* 逻辑时钟前进1单位 */
void
update_snake_pos(void) {
	int i;
	if(snakelen==0){
		/*初始化*/
		printk("game initialization\n");
		snake[snakelen].used = TRUE;
		snake[snakelen].y = ROWNUM/2;
		snake[snakelen].x = COLNUM/2;
		snake[snakelen].direction = rand()%DIRENUM;
		snakelen++;
		create_new_food();
		gamepool[snake[0].y][snake[0].x]=SNAKE;
	}
	struct snakenode temp;
	printk("game execute\n");
	i=snakelen-1;		
	temp.x=snake[i].x;
	temp.y=snake[i].y;
	for(;i>0;i--){
		snake[i].x=snake[i-1].x;
		snake[i].y=snake[i-1].y;
	}
	switch(snake[0].direction){
		case DIREU:
			/*printk("direction up\n");*/
			if(snake[0].y==0){
				ggflag=TRUE;
				return;
			}
		   	snake[0].y --; break;
		case DIRED:
			/*printk("direction down\n");*/
			if(snake[0].y==ROWNUM-1){
				ggflag=TRUE;
				return;
			}
			snake[0].y ++; break;
		case DIREL:
			/*printk("direction left\n");*/
			if(snake[0].x==0){
				ggflag=TRUE;
				return;
			}	
			snake[0].x --; break;
		case DIRER:
			/*printk("direction right\n");*/
			if(snake[0].x==COLNUM-1){
				ggflag=TRUE;
				return;
			}
			snake[0].x ++; break;
		default:
			assert(0);
	}	
	gamepool[temp.y][temp.x]=EMPTY;
	if(gamepool[snake[0].y][snake[0].x]==SNAKE) {
		printk("eat yourself!\n");
		ggflag = TRUE;	
		return;
	}
	else if(gamepool[snake[0].y][snake[0].x]==FOOD){
		printk("eat food!\n");
		snakelen++;
		snake[snakelen-1].x=temp.x;
		snake[snakelen-1].y=temp.y;
		snake[snakelen-1].used = TRUE;
		gamepool[snake[snakelen-1].y][snake[snakelen-1].x]=SNAKE;
		create_new_food();
	}	
	gamepool[snake[0].y][snake[0].x]=SNAKE;
	int j;
	for(i=0;i<ROWNUM;i++)
		for(j=0;j<COLNUM;j++)
			if(gamepool[i][j]==SNAKE)
				gamepool[i][j]=EMPTY;
	for(i=0;i<snakelen;i++)
		gamepool[snake[i].y][snake[i].x]=SNAKE;
	
}




/* 更新按键 */
bool
update_keypress(void) {
	disable_interrupt();
	int i=0;
	for(;i<DIRENUM;i++){
		if(query_direkey(i)){
			switch(i){
			case DIREL:
				if(snake[0].direction!=DIRER)snake[0].direction = i;
				break;
			case DIRER:
				if(snake[0].direction!=DIREL)snake[0].direction = i;
				break;
			case DIREU:
				if(snake[0].direction!=DIRED)snake[0].direction = i;
				break;
			case DIRED:
				if(snake[0].direction!=DIREU)snake[0].direction = i;
				break;
			default: assert(0);
			}
		
		}
		release_key(26+i);
	}
	enable_interrupt();

	return FALSE;
}

void
redraw_screen() {

	const char *len;
	
	prepare_buffer(); /* 准备缓冲区 */

	int i, j, snakenum=0;
	for(i=0;i<ROWNUM;i++)
		for(j=0;j<COLNUM;j++){
			if(gamepool[i][j]==EMPTY)	draw_block(i,j,0);
			else if(gamepool[i][j]==FOOD) draw_block(i,j,14);
			else if(gamepool[i][j]==SNAKE) {
				draw_block(i,j,10);
				snakenum++;
			}
			else assert(0);
		}
	//printk("snakenum=%d, snakelen=%d\n",snakenum,snakelen);
	assert(snakenum<=snakelen);

	/* 绘制命中数、miss数、最后一次按键扫描码和fps */
	draw_string(itoa(last_key_code()), SCR_HEIGHT - 8, 0, 48);
	
	/*hit数*/
	len = itoa(get_len());
	draw_string(len, 0, SCR_WIDTH - strlen(len) * 8, 10);
	
	/*fps*/
	draw_string(itoa(get_fps()), 0, 0, 14);
	draw_string("FPS", 0, strlen(itoa(get_fps())) * 8, 14);
	
	display_buffer(); /* 绘制缓冲区 */
}

void draw_gg(){
	prepare_buffer();
	draw_string("Game Over",SCR_HEIGHT/2-15,SCR_WIDTH/2-35,14);
	display_buffer();
	asm volatile("int $0x80": : "b"(0x103));
	printk("game over!");
}

void show_logo(){
	prepare_buffer();
	draw_logo();
	draw_string("press Space",SCR_HEIGHT/2+80,SCR_WIDTH/2-45,14);
	//asm volatile("int $0x80": : "b"(0x102));	
	display_buffer();
}


