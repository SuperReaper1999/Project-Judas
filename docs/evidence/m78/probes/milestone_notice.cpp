#include "editor/EditorMilestoneNotice.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
int main(int argc,char**argv){if(argc!=3)return 2;setenv("PATH",argv[1],1);std::atomic<bool> cancelled{std::string(argv[2])=="cancel"};auto start=std::chrono::steady_clock::now();auto r=EditorMilestoneNotice::Query(cancelled);double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();bool expected=std::string(argv[2])=="valid";bool ok=r.available==expected&&(!expected||(r.milestone==78&&r.sha==std::string(40,'a')))&&elapsed<1000;std::cout<<"{\"available\":"<<r.available<<",\"milestone\":"<<r.milestone<<",\"milliseconds\":"<<elapsed<<",\"expected\":"<<expected<<",\"ok\":"<<ok<<"}\n";return ok?0:1;}
