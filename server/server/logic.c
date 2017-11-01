/*
 Author: zerotrac
 Date: 2017/10/31
*/

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <fcntl.h>
#include <unistd.h>

#include "logic.h"
#include "userdata.h"

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
    // it seems that kqueue needs no extra unblock codes
    
    if (server_create(logic) == 1) return 1;
    if (server_bind(logic) == 1) return 1;
    //if (server_unblock(logic) == 1) return 1;
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
    struct User_data server_data;
    server_data.monitor_pos = 0;
    EV_SET(&logic->monitor_list[0], logic->server_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, &server_data);
    
    return 0;
}


int execute(struct Server_logic* logic)
{
    if (server_start(logic) == 1) return 1;
    if (kqueue_start(logic) == 1) return 1;
    
    // use infinite loop to implement server
    
    struct timespec tmout = {1, 0};
    
    while (1)
    {
        int nev = kevent(logic->kqueue_fd, logic->monitor_list, logic->kqueue_cnt, logic->trigger_list, KQUEUE_MONITOR_MAX, &tmout);
        
        if (nev < 0)
        {
            perror("kevent()");
            return 1;
        }
        
        // handle all modified kevents
        
        int modified = 0;
        
        for (int i = 0; i < nev; ++i)
        {
            struct kevent cur_event = logic->trigger_list[i];
            
            if (cur_event.flags & EV_ERROR)
            {
                int m_pos = ((struct User_data*)cur_event.udata)->monitor_pos;
                logic->monitor_list[m_pos].flags |= EV_DELETE;
                modified = 1;
            }
            else
            {
                if (cur_event.ident == logic->server_fd)
                {
                    // new connection
                    
                    struct sockaddr_in client_addr;
                    socklen_t len = sizeof client_addr;
                    int client_fd = accept(logic->server_fd, (struct sockaddr*)&client_addr, &len);
                    puts(inet_ntoa(client_addr.sin_addr));
                    if (client_fd == -1)
                    {
                        perror("client connection failed");
                    }
                    else
                    {
                        struct User_data* client_data = (struct User_data*) malloc(sizeof(struct User_data));
                        client_data->monitor_pos = logic->kqueue_cnt;
                        EV_SET(&logic->monitor_list[logic->kqueue_cnt], client_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, client_data);
                        ++logic->kqueue_cnt;
                        puts(inet_ntoa(client_addr.sin_addr));
                    }
                }
                else
                {
                    // receive data from client
                    
                    int p = 0;
                    while (1)
                    {
                        ssize_t n = recv((int)cur_event.ident, logic->recv_data + p, RECEIVE_DATA_MAX - p, 0);
                        if (n < 0)
                        {
                            // client error
                            
                            perror("read from client failed");
                            int m_pos = ((struct User_data*)cur_event.udata)->monitor_pos;
                            logic->monitor_list[m_pos].flags |= EV_DELETE;
                            modified = 1;
                            break;
                        }
                        else if (n == 0)
                        {
                            // client closed
                            
                            int m_pos = ((struct User_data*)cur_event.udata)->monitor_pos;
                            logic->monitor_list[m_pos].flags |= EV_DELETE;
                            modified = 1;
                            break;
                        }
                        
                        p += n;
                        
                        // ftp commands end with \n
                        
                        if (logic->recv_data[p - 1] == '\n') break;
                    }
                    
                    logic->recv_data[p - 2] = '\r';
                    send((int)cur_event.ident, logic->recv_data, p, 0);
                }
            }
        }
        
        // if nothing modified, continue
        
        if (!modified) continue;
        
        // delete all disconnected kevents
        
        int rep_kqueue_cnt = 0;
        for (int i = 0; i < logic->kqueue_cnt; ++i)
        {
            struct kevent cur_event = logic->monitor_list[i];
            
            if (cur_event.flags & EV_DELETE)
            {
                int fd = (int)cur_event.ident;
                free(cur_event.udata);
                cur_event.udata = NULL;
                close(fd);
            }
            else
            {
                ((struct User_data*)cur_event.udata)->monitor_pos = rep_kqueue_cnt;
                logic->trigger_list[rep_kqueue_cnt] = cur_event;
                ++rep_kqueue_cnt;
            }
        }
        logic->kqueue_cnt = rep_kqueue_cnt;
    }
    
    return 0;
}

int set_default(struct Server_logic* logic, int argc, char* argv[])
{
    // default
    logic->server_port = 21;
    strcpy(logic->server_path, "/tmp");
    
    // arguments
    
    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "-port"))
        {
            int temp_port = atoi(argv[i + 1]);
            if (temp_port <= 0)
            {
                printf("argv error: %s\n", argv[i + 1]);
                return 1;
            }
            logic->server_port = temp_port;
            ++i;
        }
        else if (!strcmp(argv[i], "-path"))
        {
            if (chdir(argv[i + 1]) == -1)
            {
                printf("argv error: %s\n", argv[i + 1]);
                return 1;
            }
            
            char* absolute_path = getcwd(NULL, 0);
            if (absolute_path == NULL)
            {
                printf("argv error: %s\n", argv[i + 1]);
                return 1;
            }
            strcpy(logic->server_path, absolute_path);
            free(absolute_path);
            ++i;
        }
        else
        {
            printf("argv error: %s\n", argv[i]);
            return 1;
        }
    }
    return 0;
}
