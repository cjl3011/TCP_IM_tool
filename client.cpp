#include<netinet/in.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<sys/epoll.h>
#include<sys/types.h>
#include<algorithm>
#include<pthread.h>
#include<iostream>
#include<unistd.h>
#include<cstdlib>
#include<cstring>
#include<errno.h>
#include<fcntl.h>
#include<cstdio>
#include<string>
#include<vector>
#include<cmath>
#include<map>
#define MAX_EVENT_NUMBER 500
#define BUFFER_SIZE 200
using namespace std;

int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, int enable_et) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if(enable_et) {
        event.events |= EPOLLET;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

map<string, int> user;
map<string, vector<string> > qun;           //群里用vector存人名
map<int, string> signin;                    //一个sock只能登录一个用户

void process_conn_server(int sock, char tmp[BUFFER_SIZE], int len) {
    char name[50], p[5], msg[50], jo[20];
    int i = 0, j = 0;
    string username;
    if(strncmp(tmp, "login", 5)!=0 && signin[sock]=="") {
        write(sock, "Please login first!\n", 20);
        return ;                            //必须先登录
    }
    if(strncmp(tmp, "login", 5) == 0) {       //输入格式为"login AAA"，可为空格但不能重复
        for(i=6,j=0; i<=strlen(tmp)-3; i++) name[j++] = tmp[i];
        name[j] = '\0';
        printf("Current username is: %s\n", name);
        username = name;
        if(user[username] == 0 && signin[sock] == "") {
            user[username] = sock; 
            write(sock, "Login success!\n", 15);
            signin[sock] = username;
        } else write(sock, "Login fail!\n", 12);        //一个终端只能登录一个用户，不能重复登录
    } else if(strncmp(tmp, "sendmsg", 7) == 0) {  //输入格式为"sendmsg pAAA BBB"代表发消息B给普通用户A，"sendmsg qAAA BBB"代表往群A发消息B
        int x = 9, y = 0, z, q, w;
        while(tmp[x] != ' ') p[y++] = tmp[x++];
        p[y] = '\0';
        for(z=0,x++; x<=strlen(tmp)-3; x++) msg[z++] = tmp[x];
        msg[z] = '\0';
        printf("Current saved msg is: %s\n", msg);
        strcat(msg, " from ");
        strcat(msg, signin[sock].c_str());
        strcat(msg, "\n");
        if(tmp[8] == 'p') {
            if(user[p] != 0) {
                write(user[p], msg, strlen(msg)+1);
                write(sock, "Send success!\n", 14);
            } else write(sock, "Send fail!\n", 11);
        } else {
            if(qun[p].size() == 0) write(sock, "Send fail!\n", 11);
            else {
                for(vector<string>::size_type s=0; s<=qun[p].size()-1; s++) 
                    write(user[qun[p][s]], msg, strlen(msg)+1); 
                write(sock, "Send success!\n", 14);
            }
        }
    } else if(strncmp(tmp, "join", 4) == 0) {      //输入格式为"join AAA"表示加入群A
            for(i=5,j=0; i<=strlen(tmp)-3; i++) jo[j++] = tmp[i];
            jo[j] = '\0';
            printf("%s wants to join group[%s].\n", signin[sock].c_str(), jo);
            vector<string>::iterator it = find(qun[jo].begin(), qun[jo].end(), signin[sock]);
            if(it == qun[jo].end()) qun[jo].push_back(signin[sock]);
            write(sock, "Join success!\n", 14);
    } else if(strncmp(tmp, "logout", 6) == 0) {     //输入格式为"logout"表示直接注销
        write(sock, "Logout success!\n", 16);
        qun[signin[sock]].clear();
        user[signin[sock]] = 0;
        signin[sock] = "";
        sock = 0; 
    } else {
        write(sock, "Input error!\n", 13);
    }
}

void et(struct epoll_event* events, int num, int epollfd, int listenfd) {
    char buf[BUFFER_SIZE], tmp[] = "Welcome!\n";
    int i;
    for(i=0; i<num; i++) {
        int sockfd = events[i].data.fd;
        if(sockfd == listenfd) {
            struct sockaddr_in client_address;
            socklen_t client_addrlength = sizeof(client_address);
            int connfd = accept(listenfd, (struct sockaddr* )&client_address, &client_addrlength);
            addfd(epollfd, connfd, 1);
            write(connfd, tmp, sizeof(tmp));
        } else if(events[i].events & EPOLLIN) {
            memset(tmp, '\0', sizeof(tmp));
            int length = 0;
            for(;;) {
                memset(buf, '\0', BUFFER_SIZE);
                int len = recv(sockfd, buf, BUFFER_SIZE-1, 0);
                if(len < 0) {
                    if(errno==EAGAIN || errno==EWOULDBLOCK) {
                        break;
                    }
                    close(sockfd);
                    break;
                } else if(len == 0) {
                    close(sockfd);
                } else {
                    strcat(tmp, buf);
                    length += len;
                }
            }
            process_conn_server(sockfd, tmp, length);
        } else
            printf("something else happened.\n");
    } 
}

int main(int argc, char* argv[]) {
    if(argc < 2) {
        printf("correct input: %s ip_address port_number", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    int ret;
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_addr.sin_addr);
    server_addr.sin_port = htons(port);
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    if(listenfd < 0) return -1;
    ret = bind(listenfd, (struct sockaddr* )&server_addr, sizeof(server_addr));
    if(ret < 0) return -1;
    ret = listen(listenfd, 5);
    if(ret < 0) return -1;
    struct epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    if(epollfd < 0) return -1;
    addfd(epollfd, listenfd, 1);
    for(;;) {
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if(ret < 0) return -1;
        et(events, ret, epollfd, listenfd);
    }
    close(listenfd);
    return 0;
}

