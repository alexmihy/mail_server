#include "server.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <errno.h>

#include <stdio.h>
#include <stdlib.h>

#include "daemon.h"
#include "options.h"

AbstractServer::AbstractServer(
    const char *a_domain, int a_port, 
    int a_max_user_count
) 
{
    domain = strdup(a_domain);
    main_socket = -1;
    port = a_port;
    
    user_count = 0;
    max_user_count = a_max_user_count;
    
    user_socket = new int [max_user_count];
    for (int i = 0; i < max_user_count; i++)
        user_socket[i] = -1;
    
    user_ip_address = new char* [max_user_count];
    memset(user_ip_address, 0, max_user_count * sizeof(char*));
}

AbstractServer::~AbstractServer() 
{
    if (domain)
        free((void*)domain);
    
    if (main_socket >= 0)
        close(main_socket);
    
    for (int i = 0; i < max_user_count; i++) {
        if (user_socket >= 0) 
            close(i);
    }
    delete [] user_socket;
    
    if (user_ip_address) {
        for (int i = 0; i < max_user_count; i++) {
            if (user_ip_address[i]) 
                free((void*)user_ip_address[i]);
        }
        delete[] user_ip_address;
    }
}

int AbstractServer::Init() 
{
    return OpenMainSocket();
}


int AbstractServer::HandleRequest() 
{
    fd_set read_fds = GetReadFds();
    int max_fd = GetMaxFd();

    struct timeval t_select= {server_options.timeout, 0};

    int res = select(max_fd + 1, &read_fds, 0, 0, &t_select);
    if (res < 0) {
        write_log(
            "[SMTP-DAEMON] select() failed\n(%s)\n", strerror(errno)
        );
        return -1;
    }
    
    if (HaveNewConnection(read_fds))
        ConnectUser();
    
    for (int i = 0; i < max_user_count; i++) {
        if ((user_socket[i] > max_fd) || (user_socket[i] == -1))
            continue;
        
        HandleOutData(i);
        
        if (HaveNewDataFromUser(i, read_fds))
            HandleInData(i);
    }
    
    return 0;
}



const char* AbstractServer::GetDomain() const { return domain; }

int AbstractServer::GetUserCount() const { return user_count; } 

int AbstractServer::GetMaxUserCount() const { return max_user_count; }

fd_set AbstractServer::GetReadFds() const 
{
    fd_set read_fds;
    
    FD_ZERO(&read_fds);
    
    FD_SET(main_socket, &read_fds);
    for (int i = 0; i < max_user_count; i++) {
        if (user_socket[i] != -1) {
            FD_SET(user_socket[i], &read_fds);
        }
    }
    
    return read_fds;
}

int AbstractServer::GetMaxFd() 
{
    int max_fd = main_socket;
    
    for (int i = 0; i < max_user_count; i++) {
        max_fd = user_socket[i] > max_fd ? user_socket[i]: max_fd;
    }
    
    return max_fd;
}

int AbstractServer::GetUserSocketFd(int user_idx) 
{
    if ((0 > user_idx) || (user_idx >= max_user_count)) 
        return -2;
    
    return user_socket[user_idx];
}

int AbstractServer::GetUserIndex(const char *ip_address) const 
{
    for (int i = 0; i < max_user_count; i++) {
        if (!strcmp(user_ip_address[i], ip_address))
            return i;
    }
    
    return -1;
}



bool AbstractServer::HaveNewConnection(fd_set &read_fds) 
{ 
    return FD_ISSET(main_socket, &read_fds);
}

bool AbstractServer::HaveNewDataFromUser(int user_idx, fd_set &read_fds) 
{
    return FD_ISSET(user_socket[user_idx], &read_fds);
}



int AbstractServer::OpenMainSocket()
{
    main_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (main_socket < 0) {
        write_log(
            "[SMTP-DAEMON] Can't open main socket\n(%s)\n", 
            strerror(errno)
        );
        return -1;
    }
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    int opt = 1;
    setsockopt(main_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    if (bind(main_socket, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        write_log(
            "[SMTP-DAEMON] Can't bind main socket\n(%s)\n", strerror(errno)
        );
        return -1;
    }
    if (listen(main_socket, 5) < 0) {
        write_log(
            "[SMTP-DAEMON] Can't switch main socket to listening mode\n(%s)\n", 
            strerror(errno)
        );
        return -1;
    }
    
    return 0;
}



void AbstractServer::ReplyToUser(int sock_fd, const char *msg) 
{
    write(sock_fd, msg, strlen(msg));
}





MailServer::MailServer(
    const char *a_domain, int a_port,
    int a_max_user_count,
    const UserList *an_user_list,
    MailQueue *a_mail_queue
) : AbstractServer(a_domain, a_port, a_max_user_count) 
{
    user_session = new SMTPProtocolServerSession* [max_user_count];
    memset(user_session, 0, max_user_count * sizeof(SMTPProtocolServerSession*));
    
    user_list = an_user_list;
    mail_queue = a_mail_queue;
}

MailServer::~MailServer() 
{
    if (user_session) {
        for (int i = 0; i < max_user_count; i++) {
            if (user_session[i])
                delete user_session[i];
        }
        
        delete [] user_session;
    }
}



const char* MailServer::ConnectUser() 
{ 
    struct sockaddr_in addr;
    socklen_t size = sizeof(addr);
    
    int sock_fd = accept(
        main_socket,
        (struct sockaddr*) &addr, 
        &size
    );

    if (sock_fd < 0) {
        return 0;
    }
    
    if (user_count >= max_user_count) {
        ReplyToUser(sock_fd, "421 ");
        ReplyToUser(sock_fd, domain);
        ReplyToUser(sock_fd, " service not available, closing transmission channel\n");
        
        shutdown(sock_fd, 2);
        close(sock_fd);
        
        return 0;
    }
    
    int user_idx;
    for (user_idx = 0; user_idx < max_user_count; user_idx++) {
        if (user_socket[user_idx] == -1) {
            user_socket[user_idx] = sock_fd;
            user_session[user_idx] = new SMTPProtocolServerSession(
                domain, 
                user_list, 
                mail_queue
            );
            user_ip_address[user_idx] = strdup(inet_ntoa(addr.sin_addr));
            user_count++;
            break;
        }
    }
    
    return user_ip_address[user_idx];
}

int MailServer::DisconnectUser(int user_idx) {
    if ((user_idx < 0) || 
        (user_idx >= max_user_count) || 
        (user_socket[user_idx] == -1)) 
        return -1;
    

    write_log(
        "[SMTP-DAEMON] IP address %s disconnected\n", 
        user_ip_address[user_idx]
    );

    shutdown(user_socket[user_idx], 2);
    close(user_socket[user_idx]);
    
    free((void*)user_ip_address[user_idx]);
    user_ip_address[user_idx] = 0;
    user_socket[user_idx] = -1;
    user_count--;
    
    return 0;
}


void MailServer::HandleInData(int user_idx) 
{ 
    if ((user_idx < 0) || 
        (user_idx >= max_user_count) || 
        (user_socket[user_idx] < 0))
        return;
    
    char buf[4096];
    int buflen;
    
    buflen = read(user_socket[user_idx], buf, sizeof(buf));
    switch (buflen) {
        case -1:
            write_log(
                "[SMTP-DAEMON] read() from user socket failed\n(%s)\n", 
                strerror(errno)
            );
            return;
        case 0:
            user_session[user_idx]->RemoteEOT();
            DisconnectUser(user_idx);
            break;
        default:
            user_session[user_idx]->EatReceivedData(buf, buflen);
    }

    if (user_session[user_idx]->ShouldWeCloseSession()) {
        DisconnectUser(user_idx);
    }
}

void MailServer::HandleOutData(int user_idx) 
{ 
    if ((user_idx < 0) || 
        (user_idx >= max_user_count) || 
        (user_socket[user_idx] < 0))
        return;
    
    char buf[4096];
    int buflen;
    
    while ((buflen = user_session[user_idx]->GetDataToTransmit(buf, sizeof(buf))) > 0) {
        int written = write(user_socket[user_idx], buf, buflen);
        switch (written) {
            case -1:
                return;
            case 0:
            default:
                user_session[user_idx]->Transmitted(written);
        }
    }
    
    if (user_session[user_idx]->ShouldWeCloseSession()) {
        DisconnectUser(user_idx);
    }
}
