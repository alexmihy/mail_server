#ifndef SMTPSRVS_H_SENTRY
#define SMTPSRVS_H_SENTRY

#include "buffer.h"
#include "userlist.h"
#include "mailqueue.h"


class AbstractProtocolServerSession 
{
protected:
    InoutBuffer inbuf;
    InoutBuffer outbuf;
private:
    bool closing_flag;
public:
    AbstractProtocolServerSession(); 
    virtual ~AbstractProtocolServerSession(); 

    void EatReceivedData(const void *buf, int len);
    bool ShouldWeCloseSession() const; 
    int GetDataToTransmit(void *buf, int buflen) const;
    void Transmitted(int len);

    virtual void RemoteEOT() = 0;

protected:
    void GracefullyClose() { closing_flag = true; }
    virtual void HandleNewData() = 0;
};

class SMTPProtocolServerSession : public AbstractProtocolServerSession 
{
    int protocols;
    enum 
    { 
        st_beforehello,
        st_waiting_authusername,
        st_waiting_authpassword,
        st_beforemail,
        st_recipients,
        st_data,
        st_closed
    } state;
    
    const UserList *user_list;
    MailQueue *mail_queue;
    
    bool authenticated;
    char *username;
    
    int recipients_count, max_recipients_count;
    char **recipients_address, *sender_address;
    InoutBuffer msg_data;
    
    bool still_accepting_data;
        
protected:
    const char *domain;
    char *remote_domain;

public:
    enum {
        smtp = 0x01,
        esmtp = 0x02,
        lmtp = 0x04
    };

    SMTPProtocolServerSession(
        const char *a_domain,
        const UserList *an_user_list,
        MailQueue *mail_queue,
        int a_protocols = smtp|esmtp|lmtp
    ); 
    virtual ~SMTPProtocolServerSession(); 

    virtual void HandleNewData();
    virtual void RemoteEOT();


protected:
    virtual void MessageDiscard();

            // must return one of:
            //      250 (Ok), 
            //      421 (Service unavailable, closing connection),
            //      451 (local error),
            //      503 (broken protocol (wrong state))
            //      553 (sender_address forbidden)
    virtual int MessageStart(const char *a_sender_address);

            // must return one of:
            //      250 (Ok)
            //      251 (Ok, but better choose another server)
            //      421 (Service unavailable, closing connection),
            //      451 (local error),
            //      450, 550 (mailbox unavailable -- not found, no access...)
            //      503 (broken protocol (wrong state))
            //      551 (relaying denied)
            //      552 (too many recipients)
            //      553 (bad address)
    virtual int MessageAddRecipient(const char *address);

            // return false if can't accept any more lines
    virtual bool MessageAddLine(const char *line);

            // [E]SMTP version
            // must return one of:
            //      250 (Ok), 
            //      451 (temporary failure)
            //      452 (quota exceeded)
            //      550 (rejection)
            //      552 (message too long)
            //      554 (permanent failure)
    virtual int MessageDataEnd();

            // LMTP version
            // Each of the responses from [0] to [n-1] must be one of:
            //      250 (Ok), 
            //      451 (temporary failure)
            //      452 (quota exceeded)
            //      550 (rejection)
            //      552 (message too long)
            //      554 (permanent failure)
            // for each of the recipients, respectively. 
            // The supplied n will always be equal to the number of 
            //   previous calls to MessageAddRecipient(...) which 
            //   returned 250 or 251.
    virtual bool MessageDataEndL(int *responses, int n)
        { for(int i = 0; i < n; responses[i++] = 250); return true; }

            // the returned string MUST NOT contain any special chars
    virtual const char * MessageCustomComment() { return 0; }

private:
    void SetRemoteDomain(const char *s);
    void ProcessData(const char *line);
    void ProcessCommand(const char *line);

    void ProcessHello(const char *param, int prot);
    void ProcessMailCommand(const char *param);
    void ProcessRcptCommand(const char *param);
    void ProcessDataCommand(const char *param);
    void ProcessRsetCommand(const char *param);
    void ProcessNoopCommand(const char *param);
    void ProcessQuitCommand(const char *param);

    void ProcessAuthCommand(const char *param);
    void FetchUsername(const char *line);
    void FetchPassword(const char *line);
    
    void UnrecognizedCommand();
    void UnimplementedCommand();
    void InternalError();
    void ServiceUnavailable();
    void DataEndResponse(int code);
    void CustomizedReply(const char *r);

    char *GenerateRecievedField(const char *message_id) const;

    int FindAtSymbolInAddress(const char *address);
};



#endif
