#include "EchoServer.hpp"

int main(){
    EchoServer server(8088);
    server.Start();
    return 0;
}