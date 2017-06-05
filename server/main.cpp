#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "server.h"
#include "daemon.h"
#include "base64/decoder.h"
#include "ipaddrlist.h"
#include "options.h"
#include "header.h"
#include "resolve.h"

int (*init_func)() = 0;
void (*main_func)() = 0;
void (*fin_func)() = 0;

Options server_options;
HeaderParser header_parser;
DNSMXResolver dns_mx_resolver;

UserList *user_list = 0;
MailQueue *mail_queue = 0;
MailServer *mail_server = 0;
IPAddressList *initial_white_list = 0, *white_list = 0, *gray_list = 0, *black_list = 0;

int Initialize() 
{
    write_log("[SMTP-DAEMON] SMTP server initialization initiated\n");

    Decoder::Init();

    if (dns_mx_resolver.Init())
        return -1;

    user_list = new UserList();
    if (user_list->Load(
            server_options.users_file, 
            server_options.users_params_file
        )) {
        return -1;
    }

    mail_queue = new MailQueue(
        server_options.domain, 
        server_options.max_messages, 
        user_list
    );
    mail_server = new MailServer(
        server_options.domain, 
        server_options.smtp_port, 
        server_options.max_connections,
        user_list, 
        mail_queue
    );

    initial_white_list = new IPAddressList();
    if (initial_white_list->Load(server_options.init_whitelist_file))
        return -1;

    white_list = new IPAddressList();
    if (white_list->Load(server_options.whitelist_file)) 
        return -1;

    gray_list = new IPAddressList();
    if (gray_list->Load(server_options.graylist_file)) 
        return -1;

    black_list = new IPAddressList();
    if (black_list->Load(server_options.blacklist_file))
        return -1;
    
    
    if (mail_server->Init())
        return -1;

    write_log("[SMTP-DAEMON] SMTP server initialization done\n");

    return 0;
}

void MainLoop() 
{
    fd_set read_fds = mail_server->GetReadFds();
    int max_fd = mail_server->GetMaxFd();

    int res = select(max_fd + 1, &read_fds, 0, 0, 0);

    if (res < 0) {
        write_log("[SMTP-DAEMON] select() failed\n(%s)\n", strerror(errno));
        exit(CHILD_NEED_WORK);
    }

    if (mail_server->HaveNewConnection(read_fds)) {
        const char *ip_address = mail_server->ConnectUser();

        write_log("[SMTP-DAEMON] New connection from %s\n", ip_address);

        if (ip_address) { 
            bool should_accept_connection = 1;
            if (black_list->FindIPAddress(ip_address) >= 0) {
                write_log("[SMTP-DAEMON] IP address %s in black list\n");
                should_accept_connection = 0;
            } else if ((initial_white_list->FindIPAddress(ip_address) < 0) && 
                (white_list->FindIPAddress(ip_address) < 0)) {
                write_log("[SMTP-DAEMON] IP address %s not in white lists\n", ip_address);
                should_accept_connection = 0;
                
                if (gray_list->FindIPAddress(ip_address) < 0) {
                    write_log("[SMTP-DAEMON] IP address %s added to gray list\n", ip_address);
                    gray_list->AddElement(ip_address);
                } else {
                    if (gray_list->ShouldAcceptConnection(ip_address)) {
                        write_log("[SMTP-DAEMON] IP address %s was in gray list\n", ip_address);
                        should_accept_connection = 1;
                        gray_list->DeleteElement(gray_list->FindIPAddress(ip_address));
                        white_list->AddElement(ip_address);
                    }
                } 
            }
            
            if (!should_accept_connection) {
                write_log("[SMTP-DAEMON] Connection from %s declined\n", ip_address);
                
                int user_idx = mail_server->GetUserIndex(ip_address);
                int sock_fd = mail_server->GetUserSocketFd(user_idx);
                
                mail_server->ReplyToUser(sock_fd, "421 ");
                mail_server->ReplyToUser(sock_fd, mail_server->GetDomain());
                mail_server->ReplyToUser(sock_fd, " service not available, closing transmission channel\n");
                
                mail_server->DisconnectUser(user_idx);
            } else {
                write_log("[SMTP-DAEMON] Connection from %s accepted\n", ip_address);   
            }
        }
    }
    
    for (int idx = 0; idx < mail_server->GetMaxUserCount(); idx++) {
        int sock_fd = mail_server->GetUserSocketFd(idx);
        if (sock_fd < 0)
            continue;
        
        if (mail_server->HaveNewDataFromUser(idx, read_fds))
            mail_server->HandleInData(idx);
    
        mail_server->HandleOutData(idx);
    }
    
    mail_queue->HandleQueue();

    gray_list->MoveRecordsToSpam(black_list);
    gray_list->DeleteOldRecords();
}

void Finalize() 
{
    if (user_list)
        delete user_list;
    
    if (mail_queue)
        delete mail_queue;
    if (mail_server)
        delete mail_server;

    if (initial_white_list)
        delete initial_white_list;
    if (white_list)
        delete white_list;
    if (gray_list)
        delete gray_list;
    if (black_list)
        delete black_list;

    write_log("[SMTP-DAEMON] SMTP server finalize done\n");
}

int main(int argc, char **argv) 
{
    printf("hello, SMTP!\n");

    init_func = Initialize;
    main_func = MainLoop;
    fin_func = Finalize;

    int status;
    status = start_daemon(argc, argv);

    if (status) {
        printf("SMTP server start aborted\n");
        return -1;
    }

    return 0;
}