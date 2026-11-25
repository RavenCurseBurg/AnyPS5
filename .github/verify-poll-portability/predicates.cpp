static void check(bool ok, const char* what) {
    if (!ok) throw std::runtime_error(std::string(what) + ", errno=" + std::to_string(errno));
}
static short hostPoll(int fd, int timeout) {
    pollfd interest{fd, POLLIN, 0};
    check(poll(&interest, 1, timeout) >= 0, "poll");
    return interest.revents;
}
static void pipeCases() {
    int fds[2];
    check(pipe(fds) == 0, "pipe");
    check(!PollDataQueued(fds[0],true,hostPoll(fds[0],0),1), "empty pipe");
    check(write(fds[1],"X",1)==1 && close(fds[1])==0,"write and close pipe");
    check(PollDataQueued(fds[0],true,hostPoll(fds[0],0),1), "bytes before EOF");
    char c;
    check(read(fds[0],&c,1)==1 && c=='X',"pipe byte");
    check(!PollDataQueued(fds[0],true,hostPoll(fds[0],0),1),"drained pipe");
    close(fds[0]);
}
static void dataCases() {
    int fds[2];
    check(socketpair(AF_UNIX,SOCK_STREAM,0,fds)==0,"stream pair");
    check(send(fds[0],"ab",2,0)==2,"two bytes");
    check(!PollDataQueued(fds[1],false,hostPoll(fds[1],0),3),"two below low water");
    check(send(fds[0],"c",1,0)==1,"third byte");
    check(PollDataQueued(fds[1],false,hostPoll(fds[1],100),3),"three at low water");
    check(!SocketErrorPending(fds[1]),"readable stream misidentified as error");
    char bytes[3];
    check(recv(fds[1],bytes,3,0)==3 && std::string(bytes,3)=="abc","peek preserves bytes");
    close(fds[0]); close(fds[1]);
    check(socketpair(AF_UNIX,SOCK_DGRAM,0,fds)==0,"datagram pair");
    check(send(fds[0],"",0,0)==0,"send empty datagram");
    check(PollDataQueued(fds[1],false,hostPoll(fds[1],100),1),"empty datagram is queued");
    check(!SocketErrorPending(fds[1]),"empty datagram misidentified as error");
    check(recv(fds[1],bytes,1,0)==0,"empty datagram was preserved");
    check(!PollDataQueued(fds[1],false,hostPoll(fds[1],0),1),"drained datagram");
    close(fds[0]); close(fds[1]);
}
static void resetCase(bool shutRead, bool graceful) {
    int listener=socket(AF_INET,SOCK_STREAM,0);
    check(listener>=0,"listener socket");
    sockaddr_in address{};
    address.sin_family=AF_INET;
    address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    check(bind(listener,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0 && listen(listener,1)==0,"listen");
    socklen_t length=sizeof(address);
    check(getsockname(listener,reinterpret_cast<sockaddr*>(&address),&length)==0,"listener port");
    int client=socket(AF_INET,SOCK_STREAM,0);
    check(client>=0 && connect(client,reinterpret_cast<sockaddr*>(&address),length)==0,"connect");
    check(PollDataQueued(listener,false,hostPoll(listener,500),1),"listener pending");
    int server=accept(listener,nullptr,nullptr);
    check(server>=0,"accept");
    check(!SocketErrorPending(server),"clean would-block");
    if (shutRead) check(shutdown(server,SHUT_RD)==0,"shut read");
    if (!graceful) {
        linger option{1,0};
        check(setsockopt(client,SOL_SOCKET,SO_LINGER,&option,sizeof(option))==0,"abortive close");
    }
    close(client);
    const auto initial=hostPoll(server,1500);
    bool first=false;
    for (unsigned i=0;i<100 && !first;++i) {
        first=SocketErrorPending(server);
        if (!first && !graceful) usleep(1000);
        if (graceful) break;
    }
    const bool second=SocketErrorPending(server);
    char c;
    errno=0;
    const auto received=recv(server,&c,1,MSG_DONTWAIT);
    const int error=errno;
    std::printf("TCP shutRead=%d graceful=%d native_revents=%d probes=%d/%d recv=%zd errno=%d\n",
                shutRead,graceful,initial,first,second,received,error);
    std::fflush(stdout);
    check(graceful ? !first && !second && received==0 : first && second && received<0 && error==ECONNRESET,"reset and EOF retention");
    close(server); close(listener);
}
int main() {
    pipeCases();
    dataCases();
    resetCase(false,true);
    resetCase(false,false);
    resetCase(true,false);
    std::puts("Native Darwin poll predicates passed");
}
