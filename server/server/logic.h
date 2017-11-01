/*
 Author: zerotrac
 Date: 2017/10/31
*/

#ifndef LOGIC_H
#define LOGIC_H

#include <sys/socket.h>
#include <sys/event.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "const.h"

struct Server_logic
{
    int server_port;
    char server_path[RECEIVE_DATA_MAX];
    
    int server_fd;
    struct sockaddr_in server_addr;
    char recv_data[RECEIVE_DATA_MAX];
    
    int kqueue_fd;
    int kqueue_cnt;
    struct kevent monitor_list[KQUEUE_MONITOR_MAX];
    struct kevent trigger_list[KQUEUE_MONITOR_MAX];
};

int server_create(struct Server_logic* logic);
int server_bind(struct Server_logic* logic);
int server_unblock(struct Server_logic* logic);
int server_listen(struct Server_logic* logic);
int server_start(struct Server_logic* logic);
int kqueue_start(struct Server_logic* logic);
int execute(struct Server_logic* logic);

int set_default(struct Server_logic* logic, int argc, char* argv[]);

#endif
