#ifndef RESOLVE_H_SENTRY
#define RESOLVE_H_SENTRY


class DNSMXResolver {
    enum 
    {
        T_MX = 15 
    };

    struct DNS_HEADER
    {
        unsigned short id; // identification number
     
        unsigned char rd :1; // recursion desired
        unsigned char tc :1; // truncated message
        unsigned char aa :1; // authoritive answer
        unsigned char opcode :4; // purpose of message
        unsigned char qr :1; // query/response flag
     
        unsigned char rcode :4; // response code
        unsigned char cd :1; // checking disabled
        unsigned char ad :1; // authenticated data
        unsigned char z :1; // its z! reserved
        unsigned char ra :1; // recursion available
     
        unsigned short q_count; // number of question entries
        unsigned short ans_count; // number of answer entries
        unsigned short auth_count; // number of authority entries
        unsigned short add_count; // number of resource entries
    };

    struct QUESTION
    {
        unsigned short qtype;
        unsigned short qclass;
    };
     
    #pragma pack(push, 1)
    struct R_DATA
    {
        unsigned short type;
        unsigned short _class;
        unsigned int ttl;
        unsigned short data_len;
    };
    #pragma pack(pop)
     
    struct RES_RECORD
    {
        unsigned char *name;
        struct R_DATA *resource;
        char *rdata;
    };

    typedef struct
    {
        unsigned char *name;
        struct QUESTION *ques;
    } QUERY;

    enum 
    {
        K_MAX_DNS_SERVER_COUNT = 10,
        K_MAX_DNS_SERVER_SIZE  = 100,
        K_BUF_SIZE = 65536,
        K_MAX_ANSWER_SIZE = 20, 
        K_PORT = 53
    };

    char dns_servers[K_MAX_DNS_SERVER_COUNT][K_MAX_DNS_SERVER_SIZE];
    int dns_server_count;

public:
    DNSMXResolver();
    ~DNSMXResolver();

    int Init();

    char* GetMXWithLowestPreference(const char *host);

private:
    int GetDNSServers();
    void ChangetoDNSNameFormat(unsigned char *dns, unsigned char *host);
    unsigned char* ReadName(
        unsigned char* reader,
        unsigned char* buffer, 
        int &count
    );

};

extern DNSMXResolver dns_mx_resolver;

#endif