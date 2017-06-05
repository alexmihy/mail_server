#ifndef IPADDRLIST_H_SENTRY
#define IPADDRLIST_H_SENTRY

#include <time.h>
#include <stdio.h>

struct IPAddressListRecord 
{
    char *ip_address;
    time_t create_time;
    
    IPAddressListRecord *next, *prev;
    
    IPAddressListRecord(const char *an_ip_address);
    IPAddressListRecord(const char *an_ip_address, time_t a_create_time);
    ~IPAddressListRecord();
};

class IPAddressList 
{
    IPAddressListRecord *head, *last;
    
    int size; 
    
    char *list_filename;
    
public:
    IPAddressList();
    ~IPAddressList();
    
    int AddElement(const char *ip_address, bool should_save = 1);
    int AddElement(IPAddressListRecord *elem, bool should_save = 1);
    int DeleteElement(int idx, bool should_save = 1);
    
    void Clear();
    void DeleteOldRecords();
    void MoveRecordsToSpam(IPAddressList *black_list);
    
    int FindIPAddress(const char *ip_address) const;
    bool ShouldAcceptConnection(const char *ip_address) const;
    
    int GetSize() const;
    const char* GetIPAddress(int idx) const;
    time_t GetCreateTime(int idx) const;
    
    int Save(const char *path) const;
    int Load(const char *path);
    
private:
    const IPAddressListRecord* GetElement(int idx) const;
    
    static char* ReadLineFromFile(FILE *f, int &len);
};


#endif