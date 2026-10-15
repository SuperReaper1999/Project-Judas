#include "InputSystem.h"
#include <cmath>
#include <cstdio>
#include <limits>
static unsigned checks=0,failures=0;
static void Check(bool ok,const char* name){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",name);}
int main(){InputSystem input;
 Check(input.Touches().empty(),"neutral contacts");
 input.SetTouch(4,8,.2f,.7f,.4f,true);auto first=input.Touches().at(0);
 Check(first.id>0&&first.x==.2f&&first.y==.7f&&first.pressure==.4f,"normalized contact snapshot");
 input.BeginFrame();input.BeginFixedStep();input.BeginFixedStep();
 Check(input.Touches().at(0).id==first.id,"contact survives render/fixed observation without consumption");
 input.SetTouch(4,8,.8f,.3f,2,true);
 Check(input.Touches().size()==1&&input.Touches()[0].id==first.id&&input.Touches()[0].pressure==1,"motion retains identity, pressure bounded");
 input.SetTouch(5,8,0,0,1,true);
 Check(input.Touches().size()==2&&input.Touches()[1].id!=first.id,"same native finger on different devices remains independent");
 input.SetTouch(4,8,0,0,0,false);
 Check(input.Touches().size()==1&&input.Touches()[0].id!=first.id,"release retires only matching contact");
 input.SetTouch(4,8,0,0,1,true);
 Check(input.Touches().back().id>first.id,"finger reuse never aliases old public contact");
 input.SetTouch(0,0,std::numeric_limits<float>::quiet_NaN(),0,1,true);
 Check(input.Touches().size()==2,"nonfinite sample rejected before mutation");
 for(int i=0;i<20;++i)input.SetTouch(0,100+i,-2,3,-1,true);
 Check(input.Touches().size()==10,"active touch capacity bounded");
 Check(input.Touches().back().x==0&&input.Touches().back().y==1&&input.Touches().back().pressure==0,"coordinates and pressure bounded");
 const auto last=input.Touches().back().id;input.Reset();
 Check(input.Touches().empty(),"focus/background reset cancels all contacts");
 input.SetTouch(0,1,0,0,1,true);Check(input.Touches()[0].id>last,"reset retains generation-safe ID sequence");
 input.ClearDevice("touch:");Check(input.Touches().empty(),"UI device ownership cancels held touches");
 input.SetTouch(0,1,0,0,1,true);Check(input.Touches()[0].id>last,"UI cancellation does not recycle contact identity");
 input.SetPointer(.3f,.7f,true);Check(input.Pointer().down&&input.Pointer().x==.3f,"primary pointer snapshot");
 input.SetPointer(-1,2,false);Check(!input.Pointer().down&&input.Pointer().x==0&&input.Pointer().y==1,"pointer clamps normalized coordinates");
 input.Reset();Check(!input.Pointer().down,"reset clears primary pointer");
 std::printf("%u checks, %u failures\n",checks,failures);return failures?1:0;}
