// Test-only Android kernel input source. No code is linked into games.
// Run as root on an isolated emulator, never on an operator's physical phone.
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static void event(int fd,int type,int code,int value){struct input_event e;memset(&e,0,sizeof(e));e.type=type;e.code=code;e.value=value;if(write(fd,&e,sizeof(e))!=sizeof(e)){perror("input write");exit(1);}}
static void axis(int fd,int code,int value){event(fd,EV_ABS,code,value);event(fd,EV_SYN,SYN_REPORT,0);}
static void button(int fd,int code,int value){event(fd,EV_KEY,code,value);event(fd,EV_SYN,SYN_REPORT,0);}
int main(){int fd=open("/dev/uinput",O_WRONLY|O_NONBLOCK);if(fd<0){perror("uinput");return 1;}
 ioctl(fd,UI_SET_EVBIT,EV_KEY);for(int key=BTN_SOUTH;key<=BTN_THUMBR;++key)ioctl(fd,UI_SET_KEYBIT,key);
 ioctl(fd,UI_SET_EVBIT,EV_ABS);for(int a=ABS_X;a<=ABS_RZ;++a)ioctl(fd,UI_SET_ABSBIT,a);
 struct uinput_user_dev d;memset(&d,0,sizeof(d));snprintf(d.name,sizeof(d.name),"M88 Synthetic Standard Gamepad");d.id.bustype=BUS_USB;d.id.vendor=0x045e;d.id.product=0x028e;d.id.version=1;
 for(int a=ABS_X;a<=ABS_RZ;++a){d.absmin[a]=-32768;d.absmax[a]=32767;d.absflat[a]=2048;}
 if(write(fd,&d,sizeof(d))!=sizeof(d)||ioctl(fd,UI_DEV_CREATE)<0){perror("create");return 1;}
 puts("created; waiting for Android InputDevice/SDL registration");fflush(stdout);sleep(3);
 axis(fd,ABS_X,26000);sleep(1);axis(fd,ABS_X,0);button(fd,BTN_SOUTH,1);usleep(120000);button(fd,BTN_SOUTH,0);puts("rightward axis intent and South delivered");fflush(stdout);sleep(2);
 axis(fd,ABS_X,-26000);sleep(1);axis(fd,ABS_X,0);sleep(1);
 ioctl(fd,UI_DEV_DESTROY);close(fd);puts("removed device; held axes cleared");return 0;}
