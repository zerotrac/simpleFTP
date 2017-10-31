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

typedef struct Server_logic
{
    int server_fd;
    struct sockaddr_in server_addr;
    char recv_data[RECEIVE_DATA_MAX];
    
    int kqueue_fd;
    int kqueue_cnt;
    struct kevent monitor_list[KQUEUE_MONITOR_MAX];
    struct kevent trigger_list[KQUEUE_MONITOR_MAX];
}Server_logic;

#endif
