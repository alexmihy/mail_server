#ifndef SERVER_H_SENTRY
#define SERVER_H_SENTRY

#include "smtpsrvs.h"

#include <sys/types.h>

class AbstractServer 
{
protected:
    int main_socket, port;
    char *domain;
    
    char **user_ip_address;
    int *user_socket;
    int user_count, max_user_count;
    
public:
    AbstractServer(
        const char *a_domain, 
        int a_port, 
        int a_max_user_count
    );
    virtual ~AbstractServer();
    
    int Init();
    
    int HandleRequest();
    
    const char* GetDomain() const;
    int GetUserCount() const;
    int GetMaxUserCount() const;
    fd_set GetReadFds() const;
    int GetMaxFd();
    int GetUserSocketFd(int user_idx);
    int GetUserIndex(const char *ip_address) const;
    
    bool HaveNewConnection(fd_set &read_fds);
    bool HaveNewDataFromUser(int user_idx, fd_set &read_fds);
    
    virtual const char* ConnectUser() = 0;
    virtual int DisconnectUser(int user_idx) = 0;
    virtual void HandleInData(int user_idx) = 0;
    virtual void HandleOutData(int user_idx) = 0;

    void ReplyToUser(int sock_fd, const char *msg);
    
private:
    int OpenMainSocket();
    
};


class MailServer: public AbstractServer 
{
    SMTPProtocolServerSession **user_session;
    
    const UserList *user_list;
    MailQueue *mail_queue;
    
public:
    MailServer(
        const char *a_domain, 
        int port, 
        int a_max_user_count,
        const UserList *an_user_list,
        MailQueue *a_mail_queue
    );
    virtual ~MailServer();
    
    
    virtual const char* ConnectUser();
    virtual int DisconnectUser(int user_idx);
    virtual void HandleInData(int user_idx);
    virtual void HandleOutData(int user_idx);
    
private:
    
};

#endif