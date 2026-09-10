// Real child process fixture; deliberately independent of manager implementation.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
static std::string environment(const char* key) { char b[4096]{}; GetEnvironmentVariableA(key,b,sizeof(b)); return b; }
int main(int argc,char** argv) {
    std::string map=argc>1?argv[1]:"",mode="ready"; int port=0;
    for(int i=1;i<argc;++i) {std::string a=argv[i];if(a.rfind("/Game/",0)==0)map=a;if(a.rfind("-port=",0)==0)port=std::stoi(a.substr(6));if(a.rfind("-fixture=",0)==0)mode=a.substr(9);}
    // Record launch environment privately for the test harness, never via the dashboard.
    const auto file=environment("GSM_FIXTURE_RECORD");
    if(!file.empty()) {std::ofstream f(file);f<<nlohmann::json({{"pid",GetCurrentProcessId()},{"nonce",environment("AURA_GSM_SERVER_READY_NONCE")},{"args",std::vector<std::string>(argv,argv+argc)}}).dump();}
    if(mode=="exit")return 23;
    if(mode=="timeout") {Sleep(60000);return 0;}
    Sleep(500);
    WSADATA data{};WSAStartup(MAKEWORD(2,2),&data);SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    sockaddr_in a{};a.sin_family=AF_INET;a.sin_port=htons(static_cast<u_short>(std::stoi(environment("AURA_GSM_PORT"))));inet_pton(AF_INET,"127.0.0.1",&a.sin_addr);
    if(connect(s,reinterpret_cast<sockaddr*>(&a),sizeof(a)))return 24;
    std::string id=map.substr(6,map.find('?')-6);
    auto request=nlohmann::json({{"action","server_ready"},{"levelId",id},{"port",port},{"readyNonce",environment("AURA_GSM_SERVER_READY_NONCE")},{"serverAuthToken",environment("AURA_GSM_SERVER_AUTH_TOKEN")}}).dump()+"\n";
    send(s,request.data(),static_cast<int>(request.size()),0);char buffer[4096];recv(s,buffer,sizeof(buffer),0);closesocket(s);WSACleanup();
    Sleep(mode=="desktop" ? 300000 : 60000);return 0;
}
