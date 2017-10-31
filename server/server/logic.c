/*
 Author: zerotrac
 Date: 2017/10/31
*/

#include <stdio.h>
#include <memory.h>
#include <fcntl.h>
#include <unistd.h>

#include "logic.h"

int server_create(struct Server_logic* logic)
{
    if ((logic->server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
    {
        perror("server create failed");
        return 1;
    }
    
    return 0;
}

int server_bind(struct Server_logic* logic)
{
    unsigned addr_sz = sizeof logic->server_addr;
    
    memset(&logic->server_addr, 0, addr_sz);
    logic->server_addr.sin_family = AF_INET;
    logic->server_addr.sin_port = 21;
    logic->server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(logic->server_fd, (struct sockaddr*)&logic->server_addr, addr_sz) == -1)
    {
        perror("server bind failed");
        return 1;
    }
    
    return 0;
}

int server_unblock(struct Server_logic* logic)
{
    int unblock_flags;
    if ((unblock_flags = fcntl(logic->server_fd, F_GETFL, 0)) == -1)
    {
        perror("server unblock get failed");
        return 1;
    }
    if (fcntl(logic->server_fd, F_SETFL, unblock_flags | O_NONBLOCK) == -1)
    {
        perror("server unblock set failed");
        return 1;
    }
    
    return 0;
}

int server_listen(struct Server_logic* logic)
{
    if (listen(logic->server_fd, SOCKET_LISTEN_MAX) == -1)
    {
        perror("server listen failed");
        return 1;
    }
    
    return 0;
}

int server_start(struct Server_logic* logic)
{
    // start a server
    // including create, bind, unblock and listen
    
    if (server_create(logic) == 1) return 1;
    if (server_bind(logic) == 1) return 1;
    if (server_unblock(logic) == 1) return 1;
    if (server_listen(logic) == 1) return 1;
    return 0;
}

int kqueue_start(struct Server_logic* logic)
{
    // start a kqueue
    // dealing with asynchronous requests
    
    if ((logic->kqueue_fd = kqueue()) == -1)
    {
        perror("kqueue create failed");
        return 1;
    }
    
    // register server socket
    
    logic->kqueue_cnt = 1;
    EV_SET(&logic->monitor_list[0], logic->server_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
    return 0;
}


int execute(struct Server_logic* logic)
{
    if (server_start(logic) == 1) return 1;
    if (kqueue_start(logic) == 1) return 1;
    puts("g3");
    
    // use infinite loop to implement server
    
    while (1)
    {
        //puts("good");
        int nev = kevent(logic->kqueue_fd, logic->monitor_list, logic->kqueue_cnt, logic->trigger_list, KQUEUE_MONITOR_MAX, NULL);
        
        //printf("nev = %d\n", nev);
        
        if (nev < 0)
        {
            perror("kevent()");
            return 1;
        }
        
        // handle all modified kevents
        
        for (int i = 0; i < nev; ++i)
        {
            struct kevent cur_event = logic->trigger_list[i];
            
            if (cur_event.flags & EV_ERROR)
            {
                for (int j = 0; j < logic->kqueue_cnt; ++j)
                {
                    if (logic->monitor_list[j].ident == cur_event.ident)
                    {
                        logic->monitor_list[j].flags |= EV_DELETE;
                        break;
                    }
                }
            }
            else
            {
                if (cur_event.ident == logic->server_fd)
                {
                    // new connection
                    
                    struct sockaddr_in client_addr;
                    //memset(&client_addr, 0, sizeof client_addr);
                    //printf("fd = %d\n", logic->server_fd);
                    int client_fd = accept(logic->server_fd, NULL, NULL);
                    //printf("client_fd = %d\n", client_fd);
                    puts(inet_ntoa(client_addr.sin_addr));
                    if (client_fd == -1)
                    {
                        //perror("client connection failed");
                    }
                    else
                    {
                        EV_SET(&logic->monitor_list[logic->kqueue_cnt], client_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
                        ++logic->kqueue_cnt;
                        puts(inet_ntoa(client_addr.sin_addr));
                    }
                }
                else
                {
                    // receive data from client
                    while (1)
                    {
                        ssize_t n = recv((int)cur_event.ident, logic->recv_data, RECEIVE_DATA_MAX, 0);
                        if (n < 0)
                        {
                            //perror("read from client failed");
                        }
                        else if (n == 0)
                        {
                            break;
                        }
                        else
                        {
                            for (int j = 0; j < n; ++j)
                            {
                                printf("%d ", logic->recv_data[j]);
                            }
                            puts("");
                        }
                    }
                }
            }
        }
        
        // delete all disconnected kevents
        
        int rep_kqueue_cnt = 0;
        for (int i = 0; i < logic->kqueue_cnt; ++i)
        {
            if (logic->trigger_list[i].flags & EV_DELETE)
            {
                int fd = (int)logic->trigger_list[i].ident;
                close(fd);
            }
            else
            {
                logic->trigger_list[rep_kqueue_cnt] = logic->trigger_list[i];
                ++rep_kqueue_cnt;
            }
        }
        logic->kqueue_cnt = rep_kqueue_cnt;
    }
    
    return 0;
}
