#ifndef MAILQUEUE_H_SENTRY
#define MAILQUEUE_H_SENTRY

#include "buffer.h"
#include "userlist.h"

#include <time.h>
#include <stdio.h>
#include <arpa/inet.h>

class Message 
{
    //MessageID
    char *id;

    //Message data
    char *sender_address, **recipients_address;
    int recipients_count;
    InoutBuffer data;

    //Path to where message is stored
    char *data_path, *info_path;
    
    //Time of the creation
    time_t create_time;
    
public:
    Message(
        const char *an_id,
        const char *a_sender_address,
        char **a_recipients_address,
        int a_recipients_count,
        const InoutBuffer *a_data
    );
    Message(
        const char *an_id,
        const char *an_info_path, 
        const char *a_data_path,
        time_t a_create_time
    );
    
    ~Message();
    
    int DeleteMessage();
    
    const char* GetId() const;
    const char* GetSenderAddress() const;
    int GetRecipientsCount() const;
    char **GetRecipientsAddress();
    const InoutBuffer* GetData() const;
    const char* GetDataPath() const;
    const char* GetInfoPath() const;
    time_t GetCreateTime() const;
    
    bool IsInfoLoaded() const;
    bool IsDataLoadad() const;

    static char* GenerateMessageId(const char *domain);
    
    static int FindAtSymbolInAddress(const char *address);
    
    int ReadDataFile();
    int ReadInfoFile();
                                    
    static char* ReadLineFromFile(FILE *f, int &len);
    
    void ReplaceInfo(
        const char *a_sender_address, 
        char **a_recipients_address, 
        int a_recipients_count
    );

    //data should exist
    int GenerateDataFile() const;
    //info should exist
    int GenerateInfoFile() const;
    
private:
    char* GenerateDataPath() const;
    char* GenerateInfoPath() const;
    char* GenerateCommonPath(int &len) const;
    
    static long unsigned int GetTimeValue(const struct tm *timeinfo);
    static long unsigned int GetRandValue(int size);
    
    static void MakeDir(const char *dir);

    void ClearInfo();
    void ClearData();
};

class MailQueue 
{
    char *domain;
    const UserList *user_list;
    
    Message **message_list;
    time_t *last_send_attempt;
    int message_count, max_message_count;
    
    time_t lifetime, sending_delay;
    
    char *queue_filename;
    
public:
    MailQueue(
        const char *a_domain,
        int a_max_message_count, 
        const UserList *an_user_list
    );
    
    ~MailQueue();
    
    
    int AddMessage(Message *message);
    int DeleteMessageFromQueue(int message_idx);
    
    int DeliverMessage(
        Message *message, 
        const char *sender_address, 
        const char *recipient_address
    );
    int SendMessage(int message_idx);
    
    void HandleQueue();
    
    int GetMessageCount() const;
    int GetMaxMessageCount() const;
    int SetMaxMessageCount(int a_max_message_count);
    void SetMaxMessageLifeTime(time_t a_lifetime);
    void SetSendingDelay(time_t a_sending_delay);
    
    int LoadQueue(const char *filename);
    int SaveQueue(const char *filename) const;
    
private:
    static char** GetDomains(
        char **recipients_address, 
        int recipients_count, 
        int *domains_count
    );
    //static int GetMXRecords(const char *name, char **mxs, int limit);
    static in_addr* GetIPAddress(const char *name);
    
    static void DisconnectFromServer(int sock_fd);
    static int ConnectToServer(in_addr *server_address);
    
    static int SendInfo(
        int sock_fd,
        const char *domain, const char *c_domain,
        const char *sender_address, 
        char **recipients_address, int recipients_count
    );
    static int SendData(
        int sock_fd, 
        const char* message_id, 
        InoutBuffer *data
    );
        
    static void WriteToSocket(int sock_fd, const char *msg, int msg_size);
    static int WriteCommandToSocket(
        int sock_fd,
        const char *comm, 
        const char *msg
    );
    static int WriteAddressToSocket(
        int sock_fd, 
        const char *comm, 
        const char *addr
    );
    static int GetReply(int sock_fd);
};

#endif







