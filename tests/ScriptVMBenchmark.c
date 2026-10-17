/* Isolates the disabled instruction-hook cost; no renderer or project changes. */
#include "quickjs.h"
#include <time.h>
#include <stdio.h>
#include <string.h>
static double now(void){struct timespec t;timespec_get(&t,TIME_UTC);return t.tv_sec+t.tv_nsec*1e-9;}
int main(void){
 JSRuntime* rt=JS_NewRuntime();JSContext* ctx=JS_NewContext(rt);
 const char* src="globalThis.run=function(){let a=1;for(let i=0;i<15000;i++){a=(a*1.00001+i)%9973;}return a;};";
 JSValue r=JS_Eval(ctx,src,strlen(src),"vm-benchmark.js",JS_EVAL_TYPE_GLOBAL);if(JS_IsException(r))return 1;JS_FreeValue(ctx,r);
 JSValue global=JS_GetGlobalObject(ctx),fn=JS_GetPropertyStr(ctx,global,"run");
 double checksum=0;
 for(int batch=0;batch<10;++batch){double start=now();for(int i=0;i<100;++i){r=JS_Call(ctx,fn,JS_UNDEFINED,0,NULL);double v=0;if(JS_ToFloat64(ctx,&v,r)<0)return 1;checksum+=v;JS_FreeValue(ctx,r);}printf("batch=%d us_per_call=%.3f checksum=%.9f\n",batch,(now()-start)*1e4,checksum);}
 JS_FreeValue(ctx,fn);JS_FreeValue(ctx,global);JS_FreeContext(ctx);JS_FreeRuntime(rt);return 0;
}
