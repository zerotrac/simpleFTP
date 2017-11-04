/*
 Author: zerotrac
 Date: 2017/10/31
*/

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <ifaddrs.h>

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
    logic->server_addr.sin_port = htons(logic->server_port);
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
    struct User_data* server_data = (struct User_data*) malloc(sizeof(struct User_data));
    User_initialize(server_data, CLIENT_COMMAND_PIPE, NULL, logic->server_fd);
    EV_SET(&logic->monitor_list[0], logic->server_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, server_data);
    
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
        
        printf("monitor list = %d\n", logic->kqueue_cnt);
        
        if (nev < 0)
        {
            perror("kevent()");
            return 1;
        }
        
        // handle all modified kevents
        
        for (int i = 0; i < nev; ++i)
        {
            struct kevent cur_event = logic->trigger_list[i];
            struct User_data* user_data = cur_event.udata;
            if (user_data->deleted || user_data->closed) continue;
            
            if (cur_event.flags & EV_ERROR)
            {
                user_data->deleted = 1;
            }
            else if (cur_event.filter == EVFILT_READ)
            {
                if (cur_event.ident == logic->server_fd)
                {
                    // new connection
                    
                    struct sockaddr_in client_addr;
                    socklen_t len = sizeof client_addr;
                    int client_fd = accept(logic->server_fd, (struct sockaddr*)&client_addr, &len);
                    if (client_fd == -1)
                    {
                        perror("client connection failed");
                    }
                    else
                    {
                        struct User_data* client_data = (struct User_data*) malloc(sizeof(struct User_data));
                        User_initialize(client_data, CLIENT_COMMAND_PIPE, NULL, client_fd);
                        EV_SET(&logic->monitor_list[logic->kqueue_cnt], client_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, client_data);
                        ++logic->kqueue_cnt;
                    }
                    send((int)client_fd, S220, strlen(S220), 0);
                }
                else
                {
                    // receive data from client
                    
                    if (user_data->client_type == CLIENT_COMMAND_PIPE)
                    {
                        ssize_t n = recv((int)cur_event.ident, logic->recv_data, RECEIVE_DATA_MAX, 0);
                        
                        printf("receive data %d bytes.\n", (int)n);
                        if (n < 0)
                        {
                            // client error
                            
                            user_data->deleted = 1;
                            break;
                        }
                        else if (n == 0)
                        {
                            // client closed
                            
                            user_data->deleted = 1;
                            break;
                        }
                        
                        logic->recv_data[n - 2] = '\0';
                        logic->recv_data[n - 1] = '\0';
                        
                        // ftp commands read finished
                        // retrieve the verb
                        
                        if (retrieve_verb(logic) == 1)
                        {
                            send((int)cur_event.ident, S500, strlen(S500), 0);
                            continue;
                        }
                        
                        printf("verb = |%s|\n", logic->command_verb);
                        printf("param = |%s|\n", logic->command_param);
                        
                        if (!strcmp(logic->command_verb, "USER"))
                        {
                            cmd_USER(logic, user_data);
                        }
                        else if (!strcmp(logic->command_verb, "PASS"))
                        {
                            cmd_PASS(logic, user_data);
                        }
                        else if (!strcmp(logic->command_verb, "QUIT"))
                        {
                            cmd_QUIT(logic, user_data);
                        }
                        else if (!strcmp(logic->command_verb, "ABOR"))
                        {
                            cmd_ABOR(logic, user_data);
                        }
                        else
                        {
                            // all of the following verbs needs login
                            
                            if (user_data->login_status != LOGIN_SUCCESS)
                            {
                                send((int)cur_event.ident, S503, strlen(S503), 0);
                                continue;
                            }
                            
                            // handle verbs other than USER and PASS
                            // with authentication
                            
                            if (!strcmp(logic->command_verb, "SYST"))
                            {
                                cmd_SYST(logic, user_data);
                            }
                            else if (!strcmp(logic->command_verb, "TYPE"))
                            {
                                cmd_TYPE(logic, user_data);
                            }
                            else if (!strcmp(logic->command_verb, "PORT"))
                            {
                                cmd_PORT(logic, user_data);
                            }
                            else if (!strcmp(logic->command_verb, "PASV"))
                            {
                                cmd_PASV(logic, user_data);
                            }
                            else if (!strcmp(logic->command_verb, "RETR"))
                            {
                                cmd_RETR(logic, user_data);
                            }
                            else if (!strcmp(logic->command_verb, "STOR"))
                            {
                                cmd_STOR(logic, user_data);
                            }
                            else if (!strcmp(logic->command_verb, "MKD"))
                            {
                                cmd_MKD(logic, user_data);
                            }
                            else if (!strcmp(logic->command_verb, "MKD"))
                            {
                                cmd_MKD(logic, user_data);
                            }
                            else if (!strcmp(logic->command_verb, "RMD"))
                            {
                                cmd_RMD(logic, user_data);
                            }
                            else if (!strcmp(logic->command_verb, "CWD"))
                            {
                                cmd_CWD(logic, user_data);
                            }
                            else if (!strcmp(logic->command_verb, "PWD"))
                            {
                                cmd_PWD(logic, user_data);
                            }
                            else if (!strcmp(logic->command_verb, "LIST"))
                            {
                                cmd_LIST(logic, user_data);
                            }
                            else
                            {
                                send((int)cur_event.ident, S500, strlen(S500), 0);
                            }
                        }
                    }
                    else if (user_data->client_type == CLIENT_ACCEPT_PIPE)
                    {
                        struct sockaddr_in addr;
                        socklen_t len = sizeof addr;
                        int client_fd = accept((int)cur_event.ident, (struct sockaddr*)&addr, &len);
                        
                        if (client_fd == -1)
                        {
                            send(user_data->connect_pt->fd, S426, strlen(S426), 0);
                            user_data->deleted = 1;
                            user_data->closed = 1;
                            close(user_data->connect_pt->fd);
                        }
                        
                        struct User_data* data = (struct User_data*) malloc(sizeof(struct User_data));
                        User_initialize(data, CLIENT_FILE_PIPE, user_data->connect_pt, client_fd);
                        strcpy(data->file_path, user_data->file_path);
                        if (user_data->which_stream == STREAM_RETR)
                        {
                            EV_SET(&logic->monitor_list[logic->kqueue_cnt], client_fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, data);
                        }
                        else
                        {
                            EV_SET(&logic->monitor_list[logic->kqueue_cnt], client_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, data);
                        }
                            ++logic->kqueue_cnt;
                        
                        user_data->connect_pt->transport_pt = data;
                    }
                    else
                    {
                        if (user_data->filefd == NULL)
                        {
                            if (!check_permission(logic, user_data->file_path))
                            {
                                send(user_data->connect_pt->fd, S451_W, strlen(S451_W), 0);
                                close_all(user_data);
                                continue;
                            }
                        }
                        
                        int cnt = (int)recv(user_data->fd, logic->recv_data, RECEIVE_DATA_MAX, 0);
                        if (cnt)
                        {
                            if (user_data->filefd == NULL)
                            {
                                user_data->filefd = fopen(user_data->file_path, "w");
                                if (user_data->filefd == NULL)
                                {
                                    send(user_data->connect_pt->fd, S451_W, strlen(S451_W), 0);
                                    close_all(user_data);
                                    continue;
                                }
                            }
                            fwrite(logic->recv_data, 1, cnt, user_data->filefd);
                        }
                        else
                        {
                            close_all(user_data);
                            fclose(user_data->filefd);
                            send(user_data->connect_pt->fd, S226, strlen(S226), 0);
                        }
                    }
                }
            }
            else if (cur_event.filter == EVFILT_WRITE)
            {
                if (user_data->filefd == NULL)
                {
                    if (!check_permission(logic, user_data->file_path) && !strstr(user_data->file_path, "list_rec.txt"))
                    {
                        send(user_data->connect_pt->fd, S451_R, strlen(S451_R), 0);
                        close_all(user_data);
                        continue;
                    }
                    user_data->filefd = fopen(user_data->file_path, "r");
                    if (user_data->filefd == NULL)
                    {
                        send(user_data->connect_pt->fd, S451_R, strlen(S451_R), 0);
                        close_all(user_data);
                        continue;
                    }
                }
                
                int cnt = (int)fread(logic->send_data, 1, RECEIVE_DATA_MAX, user_data->filefd);
                if (cnt)
                {
                    send(user_data->fd, logic->send_data, cnt, 0);
                }
                if (cnt < RECEIVE_DATA_MAX)
                {
                    close_all(user_data);
                    fclose(user_data->filefd);
                    send(user_data->connect_pt->fd, S226, strlen(S226), 0);
                }
            }
        }

        // delete all disconnected kevents
        
        int rep_kqueue_cnt = 0;
        
        for (int i = 0; i < logic->kqueue_cnt; ++i)
        {
            struct User_data* user_data = logic->monitor_list[i].udata;
            if (user_data->deleted && user_data == CLIENT_COMMAND_PIPE && user_data->transport_pt)
            {
                struct User_data* transport_data = user_data->transport_pt;
                transport_data->deleted = 1;
            }
        }
        
        for (int i = 0; i < logic->kqueue_cnt; ++i)
        {
            struct kevent cur_event = logic->monitor_list[i];
            struct User_data* user_data = cur_event.udata;
            if (user_data->deleted)
            {
                int fd = (int)cur_event.ident;
                if (!user_data->closed) close(fd);
                free(user_data);
            }
            else
            {
                logic->monitor_list[rep_kqueue_cnt] = cur_event;
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
    char* prev = getcwd(NULL, 0);
    chdir("/tmp");
    char* tmp = getcwd(NULL, 0);
    strcpy(logic->server_path, tmp);
    chdir(prev);
    free(tmp);
    free(prev);
    
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
        else if (!strcmp(argv[i], "-root"))
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
            if (!strcmp(absolute_path, "/"))
            {
                logic->server_path[0] = '\0';
            }
            else
            {
                strcpy(logic->server_path, absolute_path);
            }
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

int retrieve_verb(struct Server_logic* logic)
{
    int len = (int)strlen(logic->recv_data);
    if (!len) return 1;
    
    int space_pos = 0;
    for (int i = 0; i <= len; ++i)
    {
        if (logic->recv_data[i] == ' ' || logic->recv_data[i] == '\0')
        {
            logic->recv_data[i] = '\0';
            for (int j = 0; j < i; ++j)
            {
                logic->recv_data[j] = toupper(logic->recv_data[j]);
            }
            strcpy(logic->command_verb, logic->recv_data);
            space_pos = i;
            break;
        }
    }
    
    while (logic->recv_data[space_pos + 1] == ' ') ++space_pos;
    strcpy(logic->command_param, logic->recv_data + space_pos + 1);
    return 0;
}

int cmd_USER(struct Server_logic* logic, struct User_data* user_data)
{
    if (user_data->login_status == LOGIN_SUCCESS)
    {
        send(user_data->fd, S530_2, strlen(S530_2), 0);
    }
    else
    {
        strcpy(user_data->username, logic->command_param);
        int username_length = (int)strlen(user_data->username);
        for (int j = 0; j < username_length; ++j)
        {
            user_data->username[j] = tolower(user_data->username[j]);
        }
        user_data->login_status = LOGIN_NOPASSWORD;
        send(user_data->fd, S331, strlen(S331), 0);
    }
    return 0;
}

int cmd_PASS(struct Server_logic* logic, struct User_data* user_data)
{
    if (user_data->login_status == LOGIN_SUCCESS)
    {
        send(user_data->fd, S530_2, strlen(S530_2), 0);
    }
    else if (user_data->login_status == LOGIN_NOUSERNAME)
    {
        send(user_data->fd, S503, strlen(S503), 0);
    }
    else
    {
        if (!strcmp(user_data->username, "anonymous"))
        {
            user_data->login_status = LOGIN_SUCCESS;
            send(user_data->fd, S230, strlen(S230), 0);
        }
        else
        {
            user_data->login_status = LOGIN_NOUSERNAME;
            send(user_data->fd, S530, strlen(S530), 0);
        }
    }
    return 0;
}

int cmd_SYST(struct Server_logic* logic, struct User_data* user_data)
{
    send(user_data->fd, S215, strlen(S215), 0);
    return 0;
}

int cmd_TYPE(struct Server_logic* logic, struct User_data* user_data)
{
    if (!strcmp(logic->command_param, "I"))
    {
        send(user_data->fd, S200, strlen(S200), 0);
    }
    else
    {
        send(user_data->fd, S504, strlen(S504), 0);
    }
    return 0;
}

int cmd_QUIT(struct Server_logic* logic, struct User_data* user_data)
{
    send(user_data->fd, S221, strlen(S221), 0);
    user_data->deleted = 1;
    user_data->closed = 1;
    close(user_data->fd);
    return 0;
}

int cmd_ABOR(struct Server_logic* logic, struct User_data* user_data)
{
    send(user_data->fd, S221, strlen(S221), 0);
    user_data->deleted = 1;
    user_data->closed = 1;
    close(user_data->fd);
    return 0;
}

int cmd_PORT(struct Server_logic* logic, struct User_data* user_data)
{
    // if pasv mode previously, close it
    
    if (user_data->connection_type == CONNECTION_PASV)
    {
        close(user_data->pasvfd);
        user_data->connection_type = CONNECTION_NONE;
    }
    
    int ip1, ip2, ip3, ip4, port1, port2;
    if (sscanf(logic->command_param, "%d,%d,%d,%d,%d,%d", &ip1, &ip2, &ip3, &ip4, &port1, &port2) == 6)
    {
        user_data->connection_type = CONNECTION_PORT;
        sprintf(user_data->host, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
        user_data->port = port1 * 256 + port2;
        send(user_data->fd, S200_PORT, strlen(S200_PORT), 0);
    }
    else
    {
        send(user_data->fd, S504, strlen(S504), 0);
    }
    return 0;
}

int cmd_PASV(struct Server_logic* logic, struct User_data* user_data)
{
    // if pasv mode previously, close it
    
    if (user_data->connection_type == CONNECTION_PASV)
    {
        close(user_data->pasvfd);
        user_data->connection_type = CONNECTION_NONE;
    }
    
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    while (1)
    {
        int port = rand() % 65536;
        if (port < 20000) continue;
        
        addr.sin_port = htons(port);
        if (bind(sockfd, (struct sockaddr*)&addr, sizeof addr) == 0)
        {
            user_data->connection_type = CONNECTION_PASV;
            user_data->pasvfd = sockfd;
            
            listen(sockfd, SOCKET_LISTEN_MAX);
            
            int port1 = port / 256;
            int port2 = port % 256;
            sprintf(logic->send_data, S227, get_IP(), port1, port2);
            send(user_data->fd, logic->send_data, strlen(logic->send_data), 0);
            break;
        }
    }
    return 0;
}

int cmd_RETR(struct Server_logic* logic, struct User_data* user_data)
{
    if (user_data->connection_type == CONNECTION_NONE)
    {
        // no connection return 425
        
        send(user_data->fd, S425, strlen(S425), 0);
        return 0;
    }
    else if (!strlen(logic->command_param))
    {
        send(user_data->fd, S504, strlen(S504), 0);
    }
    else if (user_data->connection_type == CONNECTION_PORT)
    {
        // port
        
        int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(user_data->port);
        
        if (inet_pton(AF_INET, user_data->host, &addr.sin_addr) <= 0)
        {
            send(user_data->fd, S426, strlen(S426), 0);
            return 0;
        }
        
        if (connect(sockfd, (struct sockaddr*)&addr, sizeof addr) < 0)
        {
            send(user_data->fd, S426, strlen(S426), 0);
            return 0;
        }
        
        struct User_data* data = (struct User_data*) malloc(sizeof(struct User_data));
        User_initialize(data, CLIENT_FILE_PIPE, user_data, sockfd);
        if (logic->command_param[0] == '/')
        {
            sprintf(data->file_path, "%s%s", logic->server_path, logic->command_param);
        }
        else
        {
            sprintf(data->file_path, "%s%s/%s", logic->server_path, user_data->relative_path, logic->command_param);
        }
        EV_SET(&logic->monitor_list[logic->kqueue_cnt], sockfd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, data);
        ++logic->kqueue_cnt;
        
        user_data->transport_pt = data;
    }
    else
    {
        // pasv
        
        struct User_data* data = (struct User_data*) malloc(sizeof(struct User_data));
        User_initialize(data, CLIENT_ACCEPT_PIPE, user_data, user_data->pasvfd);
        if (logic->command_param[0] == '/')
        {
            sprintf(data->file_path, "%s%s", logic->server_path, logic->command_param);
        }
        else
        {
            sprintf(data->file_path, "%s%s/%s", logic->server_path, user_data->relative_path, logic->command_param);
        }
        EV_SET(&logic->monitor_list[logic->kqueue_cnt], user_data->pasvfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, data);
        ++logic->kqueue_cnt;
        
        user_data->accept_pt = data;
    }

    send(user_data->fd, S150, strlen(S150), 0);
    return 0;
}

int cmd_STOR(struct Server_logic* logic, struct User_data* user_data)
{
    if (user_data->connection_type == CONNECTION_NONE)
    {
        // no connection return 425
        
        send(user_data->fd, S425, strlen(S425), 0);
        return 0;
    }
    else if (!strlen(logic->command_param))
    {
        send(user_data->fd, S504, strlen(S504), 0);
    }
    else if (user_data->connection_type == CONNECTION_PORT)
    {
        // port
        
        int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(user_data->port);
        
        if (inet_pton(AF_INET, user_data->host, &addr.sin_addr) <= 0)
        {
            send(user_data->fd, S426, strlen(S426), 0);
            return 0;
        }
        
        if (connect(sockfd, (struct sockaddr*)&addr, sizeof addr) < 0)
        {
            send(user_data->fd, S426, strlen(S426), 0);
            return 0;
        }
        
        struct User_data* data = (struct User_data*) malloc(sizeof(struct User_data));
        User_initialize(data, CLIENT_FILE_PIPE, user_data, sockfd);
        if (logic->command_param[0] == '/')
        {
            sprintf(data->file_path, "%s%s", logic->server_path, logic->command_param);
        }
        else
        {
            sprintf(data->file_path, "%s%s/%s", logic->server_path, user_data->relative_path, logic->command_param);
        }
        EV_SET(&logic->monitor_list[logic->kqueue_cnt], sockfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, data);
        ++logic->kqueue_cnt;
        
        user_data->transport_pt = data;
    }
    else
    {
        // pasv
        
        struct User_data* data = (struct User_data*) malloc(sizeof(struct User_data));
        User_initialize(data, CLIENT_ACCEPT_PIPE, user_data, user_data->pasvfd);
        data->which_stream = STREAM_STOR;
        if (logic->command_param[0] == '/')
        {
            sprintf(data->file_path, "%s%s", logic->server_path, logic->command_param);
        }
        else
        {
            sprintf(data->file_path, "%s%s/%s", logic->server_path, user_data->relative_path, logic->command_param);
        }
        EV_SET(&logic->monitor_list[logic->kqueue_cnt], user_data->pasvfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, data);
        ++logic->kqueue_cnt;
        
        user_data->accept_pt = data;
    }
    
    send(user_data->fd, S150, strlen(S150), 0);
    return 0;
}

int cmd_MKD(struct Server_logic* logic, struct User_data* user_data)
{
    if (logic->command_param[0] == '/')
    {
        sprintf(logic->send_data, "%s%s", logic->server_path, logic->command_param);
    }
    else
    {
        sprintf(logic->send_data, "%s%s/%s", logic->server_path, user_data->relative_path, logic->command_param);
    }
    
    if (!check_permission(logic, logic->send_data))
    {
        printf("this 550\n");
        send(user_data->fd, S550, strlen(S550), 0);
    }
    else
    {
        printf("that 550\n");
        int op = mkdir(logic->send_data, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if (op == 0)
        {
            send(user_data->fd, S250, strlen(S250), 0);
        }
        else
        {
            send(user_data->fd, S550, strlen(S550), 0);
        }
    }
    return 0;
}

int cmd_RMD(struct Server_logic* logic, struct User_data* user_data)
{
    if (logic->command_param[0] == '/')
    {
        sprintf(logic->send_data, "%s%s", logic->server_path, logic->command_param);
    }
    else
    {
        sprintf(logic->send_data, "%s%s/%s", logic->server_path, user_data->relative_path, logic->command_param);
    }
    
    if (!check_permission(logic, logic->send_data))
    {
        send(user_data->fd, S550, strlen(S550), 0);
    }
    else
    {
        int op = rmdir(logic->send_data);
        if (op == 0)
        {
            send(user_data->fd, S250, strlen(S250), 0);
        }
        else
        {
            send(user_data->fd, S550_2, strlen(S550_2), 0);
        }
    }
    return 0;
}

int cmd_CWD(struct Server_logic* logic, struct User_data* user_data)
{
    if (logic->command_param[0] == '/')
    {
        sprintf(logic->send_data, "%s%s", logic->server_path, logic->command_param);
    }
    else
    {
        sprintf(logic->send_data, "%s%s/%s", logic->server_path, user_data->relative_path, logic->command_param);
    }
    
    int op = chdir(logic->send_data);
    if (op == -1)
    {
        send(user_data->fd, S550_2, strlen(S550_2), 0);
    }
    else
    {
        char* cur_path = getcwd(NULL, 0);
        if (strstr(cur_path, logic->server_path) != cur_path)
        {
            send(user_data->fd, S550, strlen(S550), 0);
        }
        else
        {
            send(user_data->fd, S250, strlen(S250), 0);
            strcpy(user_data->relative_path, cur_path + strlen(logic->server_path));
        }
        free(cur_path);
    }
    return 0;
}

int cmd_PWD(struct Server_logic* logic, struct User_data* user_data)
{
    if (!strlen(user_data->relative_path))
    {
        sprintf(logic->send_data, S257, "/");
    }
    else
    {
        sprintf(logic->send_data, S257, user_data->relative_path);
    }
    send(user_data->fd, logic->send_data, strlen(logic->send_data), 0);
    return 0;
}

int cmd_LIST(struct Server_logic* logic, struct User_data* user_data)
{
    if (user_data->connection_type == CONNECTION_NONE)
    {
        // no connection return 425
        
        send(user_data->fd, S425, strlen(S425), 0);
        return 0;
    }
    else if (user_data->connection_type == CONNECTION_PORT)
    {
        // port
        
        int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(user_data->port);
        
        if (inet_pton(AF_INET, user_data->host, &addr.sin_addr) <= 0)
        {
            send(user_data->fd, S426, strlen(S426), 0);
            return 0;
        }
        
        if (connect(sockfd, (struct sockaddr*)&addr, sizeof addr) < 0)
        {
            send(user_data->fd, S426, strlen(S426), 0);
            return 0;
        }
        
        struct User_data* data = (struct User_data*) malloc(sizeof(struct User_data));
        User_initialize(data, CLIENT_FILE_PIPE, user_data, sockfd);
        sprintf(data->file_path, "/Users/chenshuxin/Desktop/list_rec.txt");
        sprintf(logic->send_data, "ls -l %s%s > %s", logic->server_path, user_data->relative_path, data->file_path);
        system(logic->send_data);
        EV_SET(&logic->monitor_list[logic->kqueue_cnt], sockfd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, data);
        ++logic->kqueue_cnt;
        
        user_data->transport_pt = data;
    }
    else
    {
        // pasv
        
        struct User_data* data = (struct User_data*) malloc(sizeof(struct User_data));
        User_initialize(data, CLIENT_ACCEPT_PIPE, user_data, user_data->pasvfd);
        sprintf(data->file_path, "/Users/chenshuxin/Desktop/list_rec.txt");
        sprintf(logic->send_data, "ls -l %s%s > %s", logic->server_path, user_data->relative_path, data->file_path);
        system(logic->send_data);
        EV_SET(&logic->monitor_list[logic->kqueue_cnt], user_data->pasvfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, data);
        ++logic->kqueue_cnt;
        
        user_data->accept_pt = data;
    }
    
    send(user_data->fd, S150, strlen(S150), 0);
    return 0;
}

char* check_path_prefix(char* check_path)
{
    int len = (int)strlen(check_path);
    int pos = 0;
    for (int i = len - 1; i >= 0; --i)
    {
        if (check_path[i] == '/')
        {
            pos = i;
            break;
        }
    }
    char bt = check_path[pos];
    check_path[pos] = '\0';
    char* return_path = (char*)malloc(len + 1);
    strcpy(return_path, check_path);
    check_path[pos] = bt;
    return return_path;
}

int check_permission(struct Server_logic* logic, char* check_path)
{
    char* prefix = check_path_prefix(check_path);
    printf("pref = %s %s %s\n", prefix, check_path, logic->server_path);
    int op = chdir(prefix);
    if (op == -1)
    {
        free(prefix);
        return 0;
    }
    char* cur_path = getcwd(NULL, 0);
    printf("cur = %s\n", cur_path);
    if (!strlen(logic->server_path) || strstr(cur_path, logic->server_path) == cur_path)
    {
        free(prefix);
        free(cur_path);
        return 1;
    }
    free(prefix);
    free(cur_path);
    return 0;
}

char* get_IP()
{
    // copied from Shichen Liu
    
    struct ifaddrs *head = NULL, *iter = NULL;
    if (getifaddrs(&head) == -1)
    {
        return NULL;
    }
    for (iter = head; iter != NULL; iter = iter->ifa_next)
    {
        if (iter->ifa_addr == NULL)
        {
            continue;
        }
        if (iter->ifa_addr->sa_family != AF_INET)
        {
            continue;
        }
        char mask[INET_ADDRSTRLEN];
        void* ptr = &((struct sockaddr_in*)iter->ifa_netmask)->sin_addr;
        inet_ntop(AF_INET, ptr, mask, INET_ADDRSTRLEN);
        if (strcmp(mask, "255.0.0.0") == 0)
        {
            continue;
        }
        void* tmp = &((struct sockaddr_in*)iter->ifa_addr)->sin_addr;
        char* rlt = (char*)malloc(20);
        memset(rlt, 0, 20);
        inet_ntop(AF_INET, tmp, rlt, INET_ADDRSTRLEN);
        int rlt_len = (int)strlen(rlt);
        for (int i = 0; i < rlt_len; ++i)
        {
            if (rlt[i] == '.') rlt[i] = ',';
        }
        return rlt;
    }
    return NULL;
}
