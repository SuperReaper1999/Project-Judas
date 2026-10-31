#include "NetworkTransport.h"
#include <iostream>
int main(){NetworkTransport transport;std::string error;NetworkTransport::Settings settings;
 if(transport.Listen("127.0.0.1",23000,settings,error)||error.find("WebSocket")==std::string::npos)return 1;
 if(transport.Connect("127.0.0.1",23000,settings,error)||error.find("WebSocket")==std::string::npos)return 2;
 if(transport.BoundPort()!=0||transport.GetStats().connections!=0)return 3;
 transport.Poll(0);if(!transport.Events().empty())return 4;
 std::cout<<"PASS browser native-TCP rejection, diagnostic, no bound socket or peer, idle Poll\n";
}
