#include "ExperimentalBrowser.h"
#include <iostream>
#include <stdexcept>
unsigned checks=0;
void Check(bool pass){++checks;if(!pass)throw std::runtime_error("browser launch policy");}
int main(){try{
 auto run=[](std::vector<std::string> words,bool valid,bool expected,unsigned remaining){
  std::vector<char*> args;for(auto& s:words)args.push_back(s.data());
  bool enabled=false;std::string error;std::vector<char*> rest;
  const bool ok=ParseExperimentalBrowserArguments(int(args.size()),args.data(),enabled,rest,error);
  Check(ok==valid);if(ok){Check(enabled==expected);Check(rest.size()==remaining);}
  else Check(!error.empty()&&rest.empty()&&!enabled);
 };
 run({"editor"},true,false,1);
 run({"editor","game.judasproj"},true,false,2);
 run({"editor","-ExpBrowserSupport=1","game.judasproj"},true,true,2);
 run({"editor","game.judasproj","-ExpBrowserSupport","=","1"},true,true,2);
 run({"editor","-ExpBrowserSupport=0"},true,false,1);
 run({"editor","-ExpBrowserSupport=true"},false,false,0);
 run({"editor","-ExpBrowserSupport=2"},false,false,0);
 run({"editor","-ExpBrowserSupport=1","-ExpBrowserSupport=0"},false,false,0);
 run({"editor","-ExpBrowserSupport"},false,false,0);
 run({"editor","-ExpBrowserSupport","="},false,false,0);
 run({"editor","my-ExpBrowserSupport=1.judasproj"},true,false,2);
 std::cout<<"Browser option: "<<checks<<" checks, 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
