#include "Scene.h"
#include <cstdio>
int main(){std::printf("SceneObject=%zu; two copies=%zu; tiny fixture budget=%u\n",sizeof(SceneObject),sizeof(SceneObject)*2,16*1024);}
