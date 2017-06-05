#include "ipaddrlist.h"
#include "daemon.h"
#include "options.h"

#include <string.h>
#include <stdlib.h>
#include <syslog.h>

static char k_default_save_path[] = "list.txt";

IPAddressListRecord::IPAddressListRecord(const char *an_ip_address) 
{
    ip_address = strdup(an_ip_address);
    time(&create_time);
    
    next = prev = 0;
}

IPAddressListRecord::IPAddressListRecord(
    const char *an_ip_address, 
    time_t a_create_time
) 
{
    ip_address = strdup(an_ip_address);
    create_time = a_create_time;
    
    next = prev = 0;
}

IPAddressListRecord::~IPAddressListRecord() 
{
    if (ip_address) 
        free((void*)ip_address);
}





IPAddressList::IPAddressList() 
{
    head = last = 0;
    list_filename = 0;
    size = 0;
}

IPAddressList::~IPAddressList() 
{
    Clear();
    
    if (list_filename)
        free((void*)list_filename);
}



int IPAddressList::AddElement(const char *ip_address, bool should_save) 
{
    if (!size) {
        head = last = new IPAddressListRecord(ip_address);
    } else {
        last->next = new IPAddressListRecord(ip_address);
        last->next->prev = last;
        last = last->next;
    } 
    size++;
    
    if (should_save)
        Save(list_filename? list_filename: k_default_save_path);
    
    return size;
}

int IPAddressList::AddElement(IPAddressListRecord *elem, bool should_save) 
{
    if (!size) {
        head = last = elem;
    } else {
        last->next = elem;
        last->next->prev = last;
        last = last->next;
    } 
    size++;
    
    if (should_save)
        Save(list_filename? list_filename: k_default_save_path);
    
    return size;
}

int IPAddressList::DeleteElement(int idx, bool should_save) 
{
    if ((idx < 0) || (idx >= size)) 
        return -1;
    
    if (!idx) {
        if (size == 1) {
            delete head;
            head = last = 0;
        } else {
            IPAddressListRecord *new_head = head->next;
            delete head;
            head = new_head;
            head->prev = 0;
        }
    } else if (idx == size - 1) {
        IPAddressListRecord *new_last = last->prev;
        delete last;
        last = new_last;
        last->next = 0;
    } else {
        IPAddressListRecord *elem = head;
        for (int i = 0; i < idx; i++, elem = elem->next);
        
        elem->prev->next = elem->next;
        elem->next->prev = elem->prev;
        
        delete elem;
    } 
    size--;
    
    if (should_save)
        Save(list_filename? list_filename: k_default_save_path);
    
    return size;
}



void IPAddressList::Clear() 
{
    while (head) {
        IPAddressListRecord *tmp = head->next;
        delete head;
        head = tmp;
    }
    
    head = last = 0;
    size = 0;
    
    //Save(list_filename? list_filename: "list.txt");
}

void IPAddressList::DeleteOldRecords() 
{
    time_t c_time;
    time(&c_time);
    
    IPAddressListRecord *elem = head;
    for (int i = 0; elem; i++, elem = elem->next) {
        if (c_time - elem->create_time >= server_options.max_time_to_store)
            DeleteElement(i);
    }
    
    Save(list_filename? list_filename: k_default_save_path);
}

void IPAddressList::MoveRecordsToSpam(IPAddressList *black_list)
{
    time_t c_time;
    time(&c_time);

    IPAddressListRecord *elem = head;
    for (int i = 0; elem; i++, elem = elem->next) {
        if (c_time - elem->create_time >= server_options.max_time_to_pass) {
            black_list->AddElement(elem->ip_address);
            DeleteElement(i);
        }
    }

    Save(list_filename? list_filename: k_default_save_path);
}



int IPAddressList::FindIPAddress(const char *ip_address) const 
{
    int result = -1;
    
    IPAddressListRecord *elem = head;
    for (int i = 0; elem; i++, elem = elem->next) {
        if (!strcmp(elem->ip_address, ip_address)) {
            result = i;
            break;
        }
    }
    
    return result;
}

bool IPAddressList::ShouldAcceptConnection(const char *ip_address) const 
{
    time_t c_time;
    time(&c_time);
    
    const IPAddressListRecord *elem = GetElement(FindIPAddress(ip_address));

    return !elem? 0 : 
        (elem->create_time < 0) || 
        (c_time - elem->create_time >= server_options.min_time_to_pass);
}



int IPAddressList::GetSize() const 
{
    return size;
}

const char* IPAddressList::GetIPAddress(int idx) const 
{
    if ((idx < 0) || (idx >= size))
        return 0;
    
    return GetElement(idx)->ip_address;
}

time_t IPAddressList::GetCreateTime(int idx) const 
{
    if ((idx < 0) || (idx >= size))
        return 0;
    
    return GetElement(idx)->create_time;
}



int IPAddressList::Save(const char *path) const
{
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        write_log(
            "[SMTP-DAEMON] Can't open %s file to save ipaddrlist\n", 
            path
        );
        return -1;
    }
        
    fprintf(f, "%d\n", size);
    for (IPAddressListRecord *elem = head; elem; elem = elem->next) {
        fprintf(f, "%s\n", elem->ip_address);
        fprintf(f, "%ld\n", elem->create_time);
    }
    fclose(f);
    
    return 0;
}

int IPAddressList::Load(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        write_log(
            "[SMTP-DAEMON] Can't open %s file to load ipaddrlist\n", 
            path
        );
        return -1;
    }
    
    list_filename = strdup(path);
    
    time_t create_time;
    int len, count;
    char *ip_address;
    
    if (fscanf(f, "%d\n", &count) < 1) 
        return -1;
    
    for (int i = 0; i < count; i++) {
        ip_address = ReadLineFromFile(f, len);
        fscanf(f, "%ld\n", &create_time);
        
        AddElement(new IPAddressListRecord(ip_address, create_time), 0);
        delete[] ip_address;
    }
    fclose(f);
    
    return 0;
}



const IPAddressListRecord* IPAddressList::GetElement(int idx) const 
{
    if ((idx < 0) || (idx >= size))
        return 0;
    
    const IPAddressListRecord* elem = head;
    for (int i = 0; i < idx; i++, elem = elem->next);
    
    return elem;
}

char* IPAddressList::ReadLineFromFile(FILE *f, int &len) 
{
    int max_len = 256;
    char *line = new char [max_len];
    len = 0;
    
    char c;
    while (fscanf(f, "%c", &c) > 0) {
        if (len + 1 >= max_len) {
            char *new_line = new char [max_len * 2];
            for (int i = 0; i < len; i++)
                new_line[i] = line[i];
            
            delete[] line;
            
            line = new_line;
            max_len *= 2;
        }
        
        if (c == '\n')
            break;
        line[len++] = c;
    }
    line[len] = '\0';
    
    if (!len) {
        delete[] line;
        line = 0;
    }
    
    return line;
}
