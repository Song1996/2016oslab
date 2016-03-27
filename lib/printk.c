#include "./include/common.h"

/* implement this function to support printk */
void vfprintf(void (*printer)(char), const char *ctl, void **args) {	
	const char* str = ctl;
	char numchar[12];
	int argn = 0;
	for(;*str != '\0'; str ++){
		if(*str!='%')printer(*str);
		else{
			str++;
			if(*str=='d'){
				int temp = (int)(args[argn++]);
				int i=0;
				if(temp > 0){
					while(temp){
						numchar[i++]=(char)(temp%10)+'0';
						temp/=10;
					}
					i--;
					for(;i>=0;i--)printer(numchar[i]);
				} else if(temp == 0){
					printer('0');
				} else if(temp == 0x80000000){
					printer('-');
					printer('2');printer('1');printer('4');printer('7');
					printer('4');printer('8');printer('3');printer('6');
					printer('4');printer('8');
				} else if(temp < 0){
					temp = -temp;
					while(temp){
						numchar[i++]=(char)(temp%10)+'0';
						temp/=10;
					}
					i--;
					printer('-');
					for(;i>=0;i--)printer(numchar[i]);
				}
			} else if(*str=='x'){
				unsigned temp = (unsigned)args[argn++];
				int i=0;
				while(temp){
					if(temp%16<10)
						numchar[i++]=temp%16+'0';
					else
						numchar[i++]=temp%16-10+'a';
					temp/=16;
				}
				for(i--;i>=0;i--)printer(numchar[i]);	
			} else if(*str=='c'){
				char temp = (unsigned)args[argn++];
				printer(temp);
			} else if(*str=='s'){	
				const char* temp = (char*)args[argn++];
				while(*temp!='\0'){
					printer(*temp);
					temp++;
				}
			} else if(*str=='%'){
				printer('%');
			} else assert(0);
		}
	}

}

extern void serial_printc(char);

/* __attribute__((__noinline__))  here is to disable inlining for this function to avoid some optimization problems for gcc 4.7 */
void __attribute__((__noinline__)) 
printk(const char *ctl, ...) {
	void **args = (void **)&ctl + 1;
	vfprintf(serial_printc, ctl, args);
}
