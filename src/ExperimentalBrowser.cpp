#include "ExperimentalBrowser.h"

bool ParseExperimentalBrowserArguments(int argc,char** argv,bool& enabled,
                                      std::vector<char*>& remaining,std::string& error) {
    bool candidate=false,seen=false;
    std::vector<char*> filtered;
    if(argc>0)filtered.push_back(argv[0]);
    for(int i=1;i<argc;++i) {
        const std::string option=argv[i];
        std::string value;
        if(option=="-ExpBrowserSupport") {
            if(i+1<argc&&std::string(argv[i+1])=="=")++i;
            if(i+1>=argc){error="Use -ExpBrowserSupport=1 or -ExpBrowserSupport=0";return false;}
            value=argv[++i];
        } else if(option.rfind("-ExpBrowserSupport=",0)==0) {
            value=option.substr(std::string("-ExpBrowserSupport=").size());
        } else {filtered.push_back(argv[i]);continue;}
        if(seen||!(value=="0"||value=="1")) {
            error="Specify -ExpBrowserSupport once, with exactly 0 or 1";return false;
        }
        seen=true;candidate=value=="1";
    }
    enabled=candidate;remaining=std::move(filtered);error.clear();return true;
}
