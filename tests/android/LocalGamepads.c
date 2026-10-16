// Emulator-only native kernel input fixture, never linked into Judas games.
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static void emit(int fd,int type,int code,int value){struct input_event e;memset(&e,0,sizeof(e));e.type=type;e.code=code;e.value=value;if(write(fd,&e,sizeof(e))!=sizeof(e)){perror("event");exit(1);}if(type!=EV_SYN)emit(fd,EV_SYN,SYN_REPORT,0);}
static int create(int seat){int fd=open("/dev/uinput",O_WRONLY|O_NONBLOCK);if(fd<0){perror("uinput");exit(1);}ioctl(fd,UI_SET_EVBIT,EV_KEY);for(int k=BTN_SOUTH;k<=BTN_THUMBR;k++)ioctl(fd,UI_SET_KEYBIT,k);ioctl(fd,UI_SET_EVBIT,EV_ABS);for(int a=ABS_X;a<=ABS_RZ;a++)ioctl(fd,UI_SET_ABSBIT,a);struct uinput_user_dev d;memset(&d,0,sizeof(d));snprintf(d.name,sizeof(d.name),"M89 Synthetic Gamepad %d",seat);d.id.bustype=BUS_USB;d.id.vendor=0x045e;d.id.product=0x028e;d.id.version=seat;for(int a=ABS_X;a<=ABS_RZ;a++){d.absmin[a]=-32768;d.absmax[a]=32767;d.absflat[a]=2048;}if(write(fd,&d,sizeof(d))!=sizeof(d)||ioctl(fd,UI_DEV_CREATE)<0){perror("create");exit(1);}return fd;}
static void mark(const char* s){puts(s);fflush(stdout);}
int main(){int a=create(1),b=create(2);mark("CREATED_TWO");sleep(4);emit(a,EV_ABS,ABS_X,26000);emit(b,EV_ABS,ABS_X,-26000);mark("DRIVING_OPPOSITE");sleep(2);ioctl(a,UI_DEV_DESTROY);close(a);mark("FIRST_DISCONNECTED");sleep(1);emit(b,EV_ABS,ABS_X,26000);emit(b,EV_KEY,BTN_SOUTH,1);usleep(120000);emit(b,EV_KEY,BTN_SOUTH,0);mark("SECOND_STILL_DRIVING");sleep(2);emit(b,EV_ABS,ABS_X,0);ioctl(b,UI_DEV_DESTROY);close(b);mark("REMOVED_TWO");return 0;}
