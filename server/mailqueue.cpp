#include "mailqueue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
       
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "base64/decoder.h"
#include "daemon.h"
#include "options.h"
#include "resolve.h"

Message::Message(
    const char *an_id, 
    const char *a_sender_address,
    char **a_recipients_address,
    int a_recipients_count,
    const InoutBuffer *a_data
) 
{
    id = strdup(an_id);

    time(&create_time);
    
    sender_address = strdup(a_sender_address);
    recipients_count = a_recipients_count;
    recipients_address = new char* [recipients_count];
    for (int i = 0; i < recipients_count; i++) 
        recipients_address[i] = strdup(a_recipients_address[i]);
    data.AddData(a_data->GetBuffer(), a_data->Length());


    data_path = GenerateDataPath();
    info_path = GenerateInfoPath();
    
    //Check for creation!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    GenerateDataFile();    
    GenerateInfoFile();
}

Message::Message(
    const char *an_id, 
    const char *an_info_path, 
    const char *a_data_path,
    time_t a_create_time
) 
{
    id = strdup(an_id);
    create_time = a_create_time;
    
    sender_address = 0;
    recipients_address = 0;
    recipients_count = 0;

    info_path = strdup(an_info_path);
    data_path = strdup(a_data_path);
}

Message::~Message() 
{
    if (id)
        free((void*)id);

    if (sender_address)
        free((void*)sender_address);

    if (recipients_address) {
        for (int i = 0; i < recipients_count; i++)
            if (recipients_address[i])
                free((void*)recipients_address[i]);
        delete [] recipients_address;
    }

    if (info_path)
        delete [] info_path;
    if (data_path)
        delete [] data_path;
}



int Message::DeleteMessage() 
{
    return remove(data_path) & remove(info_path);
}



const char* Message::GetId() const { return id; }

const char* Message::GetSenderAddress() const { return sender_address; }

int Message::GetRecipientsCount() const { return recipients_count; }

char** Message::GetRecipientsAddress() { return recipients_address; }

const InoutBuffer* Message::GetData() const { return &data; }

const char* Message::GetDataPath() const { return data_path; }

const char* Message::GetInfoPath() const { return info_path; }

time_t Message::GetCreateTime() const { return create_time; }


bool Message::IsInfoLoaded() const 
{
    return (sender_address != 0) && (recipients_address != 0);
}

bool Message::IsDataLoadad() const 
{
    return data.Length() != 0;
}

    
char* Message::GenerateMessageId(const char *domain) 
{
    time_t c_time;
    time(&c_time);
    
    struct tm *timeinfo;
    timeinfo = localtime(&c_time);
    
    long unsigned int time_val = GetTimeValue(timeinfo);
    long unsigned int rand_val = GetRandValue(16);
    
    char *time_val_base36 = Decoder::Base36Encode(time_val);
    size_t time_val_base36_len = strlen(time_val_base36);
    char *rand_val_base36 = Decoder::Base36Encode(rand_val);
    size_t rand_val_base36_len = strlen(rand_val_base36);
    
    char *id = new char [
        time_val_base36_len + 1 +
        rand_val_base36_len + 1 +
        strlen(domain) + 1
    ];

    int p = 0;
    for (size_t i = 0; i < time_val_base36_len; i++) 
        id[p++] = time_val_base36[i];
    id[p++] = '.';
    for (size_t i = 0; i < rand_val_base36_len; i++) 
        id[p++] = rand_val_base36[i];
    id[p++] = '@';
    for (size_t i = 0; i < strlen(domain); i++)
        id[p++] = domain[i];
    id[p] = '\0';
    
    free((void*)time_val_base36);
    free((void*)rand_val_base36);
    
    return id;
}

int Message::FindAtSymbolInAddress(const char *address) 
{
    int res_pos = -1, at_count = 0;
    for (int i = 0; address[i] != '\0'; i++) {
        if (address[i] == '@')
            res_pos = i, at_count++;
    }
    
    return at_count == 1 ? res_pos: -1;
}



int Message::ReadInfoFile() 
{
    FILE *f_info = fopen(info_path, "r");
    if (f_info == NULL)
        return -1;
    
    ClearInfo();

    int len;
    sender_address = ReadLineFromFile(f_info, len);
    fscanf(f_info, "%d\n", &recipients_count);
    
    recipients_address = new char* [recipients_count];
    for (int i = 0; i < recipients_count; i++)
        recipients_address[i] = ReadLineFromFile(f_info, len);
    
    fclose(f_info);
    return 0;
}

int Message::ReadDataFile()
{ 
    FILE *f_data = fopen(data_path, "r");
    if (f_data == NULL)
        return -1;
    
    ClearData();

    char c;
    while (fscanf(f_data, "%c", &c) > 0) {
        data.AddChar(c);
    }

    fclose(f_data);
    return 0;
}



char* Message::GenerateDataPath() const 
{
    int len;
    char *path = GenerateCommonPath(len);
    
    MakeDir(path);
    
    memcpy(path + len, "data.txt", 8);
    path[len + 8 + 1] = '\0';
    
    return path;
}

char* Message::GenerateInfoPath() const 
{
    int len;
    char *path = GenerateCommonPath(len);
    
    MakeDir(path);
    
    memcpy(path + len, "info.txt", 8);
    path[len + 8 + 1] = '\0';
    
    return path;
}

char* Message::GenerateCommonPath(int &len) const 
{
    int max_len = 512;
    char *buf = new char [max_len];
    
    len = strlen(server_options.queue_dir);
    memcpy(buf, server_options.queue_dir, len);
    
    int at_pos = FindAtSymbolInAddress(id);
    for (int i = 0; i < at_pos; i++) {
        if ((('A' <= id[i]) && (id[i] <= 'Z')) || (('0' <= id[i]) && (id[i] <= '9'))) {
            buf[len++] = id[i];
            buf[len++] = '/';
        }
    }
    buf[len] = '\0';
    
    return buf;
}



void Message::ReplaceInfo(
    const char *a_sender_address, 
    char **a_recipients_address, 
    int a_recipients_count
)
{
    ClearInfo();

    sender_address = strdup(a_sender_address);

    recipients_count = a_recipients_count;
    recipients_address = new char* [recipients_count];
    for (int i = 0; i < recipients_count; i++)
        recipients_address[i] = strdup(a_recipients_address[i]);
}


int Message::GenerateDataFile() const
{
    FILE *f_data = fopen(data_path, "w");
    if (f_data == NULL)
        return -1;
    
    InoutBuffer curline, cdata;
    cdata.AddData(data.GetBuffer(), data.Length());
    while (cdata.ReadLine(curline))
        fprintf(f_data, "%s\n", curline.GetBuffer());
    fclose(f_data);
    
    return 0;
}

int Message::GenerateInfoFile() const
{
    FILE *f_info = fopen(info_path, "w");
    if (f_info == NULL)
        return -1;
    
    fprintf(f_info, "%s\n%d\n", sender_address, recipients_count);
    for (int i = 0; i < recipients_count; i++) 
        fprintf(f_info, "%s\n", recipients_address[i]);
    fclose(f_info);
    
    return 0;
}



long unsigned int Message::GetTimeValue(const struct tm *timeinfo)
{
    long unsigned int time_val = 0;
    
    time_val += timeinfo->tm_sec;
    time_val += timeinfo->tm_min * 100;
    time_val += timeinfo->tm_hour * 10000;
    time_val += timeinfo->tm_mday * 1000000;
    time_val += timeinfo->tm_mon * 100000000;
    time_val += (1900 + timeinfo->tm_year) * 10000000000;
    
    return time_val;
}

long unsigned int Message::GetRandValue(int size) 
{
    long unsigned int rand_value = 0, d_deg = 1;
    
    for (int i = 0; i < size; i++) {
        rand_value += (rand() % 10) * d_deg;
        d_deg *= 10;
    }
    
    return rand_value;
}



char* Message::ReadLineFromFile(FILE *f, int &len) 
{
    int max_len = 256;
    char *line = (char*)malloc(max_len * sizeof(*line));
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


void Message::MakeDir(const char *dir) 
{
    char tmp[256];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp),"%s",dir);
    len = strlen(tmp);
    if(tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for(p = tmp + 1; *p; p++)
        if(*p == '/') {
            *p = 0;
            mkdir(tmp, S_IRWXU);
            *p = '/';
        }
    mkdir(tmp, S_IRWXU);
}

void Message::ClearInfo() 
{
    if (sender_address)
        free((void*)sender_address);
    if (recipients_address) {
        for (int i = 0; i < recipients_count; i++) 
            if (recipients_address[i])
                free((void*)recipients_address[i]);
        delete [] recipients_address;
    }

    sender_address = 0;
    recipients_address = 0;
    recipients_count = 0;
}

void Message::ClearData() 
{
    data.DropAll();
}








MailQueue::MailQueue(
    const char *a_domain, 
    int a_max_message_count, 
    const UserList *an_user_list
) 
{ 
    domain = strdup(a_domain);
    user_list = an_user_list;
    
    message_count = 0;
    max_message_count = a_max_message_count;
    message_list = new Message* [max_message_count];
    memset(message_list, 0, max_message_count * sizeof(Message*));
    
    last_send_attempt = new time_t [max_message_count];
    memset(last_send_attempt, 0, max_message_count * sizeof(time_t));
    
    lifetime = 4 * 24 * 60 * 60;
    sending_delay = 30 * 60;
}

MailQueue::~MailQueue() 
{
    if (domain)
        free((void*)domain);

    if (message_list) {
        for (int i = 0; i < max_message_count; i++) {
            if (message_list[i]) 
                delete message_list[i];
        }
        delete[] message_list;
    }

    if (last_send_attempt)
        delete [] last_send_attempt;
}



int MailQueue::AddMessage(Message *message) 
{ 
    write_log(
        "[SMTP-DAEMON] New message %s come into mail queue\n",
        message->GetId()
    );

    if (message_count >= max_message_count) {
        write_log(
            "[SMTP-DAEMON] Message %s rejected due mail queue is full\n",
            message->GetId()
        );
        return -1;
    }

    if (!message->IsInfoLoaded()) {
        if (message->ReadInfoFile() < 0) {
            write_log(
                "[SMTP-DAEMON] Message %s rejected due its info file read failed\n",
                message->GetId()
            );
            return -1;
        }
    }

    const char *sender_address = message->GetSenderAddress();
    int recipients_count = message->GetRecipientsCount();
    char **recipients_address = message->GetRecipientsAddress();


    int far_recipients_count = 0;
    char **far_recipients_address = new char* [recipients_count];
    memset(far_recipients_address, 0, recipients_count * sizeof(char*));
    
    for (int i = 0; i < recipients_count; i++) {
        int atpos = Message::FindAtSymbolInAddress(recipients_address[i]);
        if (!strcmp(recipients_address[i] + atpos + 1, domain)) {
            int status = DeliverMessage(message, sender_address, recipients_address[i]);

            if (!status) {
                write_log(
                    "[SMTP-DAEMON] Message %s local delivered to %s\n",
                    message->GetId(),
                    recipients_address[i]
                );
            } else if (status == 1) {
                write_log("[SMTP-DAEMON] Message %s replaced\n", message->GetId());
            } else {
                write_log(
                    "[SMTP-DAEMON] Error: Message %s "
                    "local delivery to %s\n failed",
                    message->GetId(),
                    recipients_address[i]
                );
            }

        } else {
            far_recipients_address[far_recipients_count++] = strdup(recipients_address[i]);
        } 
    }
    
    if (far_recipients_count > 0) {
        char *new_sender_address = strdup(sender_address);
        message->ReplaceInfo(
            new_sender_address, 
            far_recipients_address, 
            far_recipients_count
        );
        message->GenerateInfoFile();
        //message->ReadInfoFile();
        free((void*)new_sender_address);

        for (int i = 0; i < max_message_count; i++) {
            if (!message_list[i]) {
                message_list[i] = message;
                break;
            }
        }
        message_count++;

        write_log(
            "[SMTP-DAEMON] Message %s added to mail queue\n", 
            message->GetId()
        );
        
        SaveQueue(server_options.queue_file);
    }
    
    for (int i = 0; i < far_recipients_count; i++) 
        free((void*)far_recipients_address[i]);
    delete [] far_recipients_address;

    return message_count;
}

int MailQueue::DeleteMessageFromQueue(int message_idx) 
{
    if ((message_idx < 0) || (message_idx >= max_message_count))
        return -1;
    if (!message_list[message_idx])
        return -2;

    message_list[message_idx]->DeleteMessage();

    delete message_list[message_idx];
    message_list[message_idx] = 0;
    last_send_attempt[message_idx] = 0;
    message_count--;

    return 0;
}



int MailQueue::DeliverMessage(
    Message *message,
    const char *sender_address, const char *recipient_address
) 
{ 
    write_log(
        "[SMTP-DAEMON] Message %s local delivery initiated\n",
        message->GetId()
    );

    int user_idx = user_list->FindUserByAddress(recipient_address);
    if (user_idx < 0) {
        write_log(
            "[SMTP-DAEMON] Error: could not find %s recipient address\n", 
            recipient_address
        );
        return -1;
    }
    
    if (!message->IsDataLoadad()) {
        if (message->ReadDataFile() < 0) {
            write_log("[SMTP-DAEMON] Error: could not read data file\n");
            return -1;
        }
    }
    
    if (user_list->GetRedirectPath(user_idx)) {
        //return DeliverMessage(message, sender_address, user_list->GetRedirectPath(user_idx));
        
        if (message_count >= max_message_count)
            SetMaxMessageCount(2 * max_message_count);
        
        char **recipients_address = new char* [1];
        recipients_address[0] = strdup(user_list->GetRedirectPath(user_idx));
        
        Message *new_message = new Message(
            message->GetId(),
            sender_address, 
            recipients_address, 1,
            message->GetData()
        );
        AddMessage(new_message);
        
        free((void*)recipients_address[0]);
        delete[] recipients_address;
        
        return 1;
    }
    
    const char *mail_path = user_list->GetPathToMail(user_idx);
    FILE *f = fopen(mail_path, "a");
    if (f == 0) {
        write_log(
            "[SMTP-DAEMON] Message %s local delivery failed due spool file opening fail\n",
            message->GetId()
        );

        return -1;
    }
    
    fprintf(f, "MAIL FROM: %s\n", sender_address);
    fprintf(f, "RCPT TO: %s\n", recipient_address);
    fprintf(f, "DATA\n");
    
    InoutBuffer curline, data;
    data.AddData(message->GetData()->GetBuffer(), message->GetData()->Length());
    while (data.ReadLine(curline))
        fprintf(f, "%s\n", curline.GetBuffer());
    fprintf(f, ".\n\n");
    
    fclose(f);
    
    write_log(
        "[SMTP-DAEMON] Message %s local delivery done\n", 
        message->GetId()
    );

    return 0;
}

int MailQueue::SendMessage(int message_idx) 
{ 
    write_log(
        "[SMTP-DAEMON] Message %s sending initiate\n",
        message_list[message_idx]->GetId()
    );

    if (!message_list[message_idx]->IsInfoLoaded()) {
        if (message_list[message_idx]->ReadInfoFile() < 0)
            return -1;
    }

    if (!message_list[message_idx]->IsDataLoadad())
        if (message_list[message_idx]->ReadDataFile() < 0)
            return -1;

    const char *sender_address = message_list[message_idx]->GetSenderAddress();
    int recipients_count = message_list[message_idx]->GetRecipientsCount();
    char **recipients_address = message_list[message_idx]->GetRecipientsAddress();

    InoutBuffer data;
    data.AddData(
        message_list[message_idx]->GetData()->GetBuffer(), 
        message_list[message_idx]->GetData()->Length()
    );


    int domains_count;
    char **domains = GetDomains(
        recipients_address, 
        recipients_count, 
        &domains_count
    );
    
    bool *domains_done = new bool [domains_count];
    memset(domains_done, 0, domains_count * sizeof(bool));
    
    for (int i = 0; i < domains_count; i++) {
        char *mx_record;
        if ((mx_record = dns_mx_resolver.GetMXWithLowestPreference(domains[i])) == 0)
            continue;

        //write_log("MX = (%s)\n", mx_record);
        
        in_addr *ip_addr = GetIPAddress(mx_record);
        delete [] mx_record;
        
        int sock_fd = ConnectToServer(ip_addr);
        if (sock_fd < 0)
            continue;

        GetReply(sock_fd);

        int domain_recipients_count = 0;
        char **domain_recipients = new char* [recipients_count];
        
        for (int j = 0; j < recipients_count; j++) {
            int atpos = Message::FindAtSymbolInAddress(recipients_address[j]);
            if (!strcmp(recipients_address[j] + atpos + 1, domains[i]))
                domain_recipients[domain_recipients_count++] = recipients_address[j];
        }

        if (SendInfo(
                sock_fd, 
                domain, domains[i], 
                sender_address, 
                domain_recipients, domain_recipients_count
            ) < 0) {
            DisconnectFromServer(sock_fd);
            delete [] domain_recipients;
            continue;
        }

        if (SendData(sock_fd, message_list[message_idx]->GetId(), &data) < 0) {
            DisconnectFromServer(sock_fd);
            delete [] domain_recipients;
            continue;
        }
        
        WriteToSocket(sock_fd, "QUIT\r\n", 6);
        DisconnectFromServer(sock_fd);
        delete [] domain_recipients;
        
        domains_done[i] = 1;
    }
    
    bool all_domains_done = 1;
    for (int i = 0; i < domains_count; i++) {
        if (!domains_done[i]) {
            all_domains_done = 0;
            break;
        }
    }
    
    if (!all_domains_done) {
        int new_recipients_count = 0;
        char **new_recipients_address = new char* [recipients_count];
        
        for (int i = 0; i < domains_count; i++) {
            if (domains_done[i])
                continue;
            
            for (int j = 0; j < recipients_count; j++) {
                int atpos = Message::FindAtSymbolInAddress(recipients_address[j]);
                if (!strcmp(recipients_address[j] + atpos + 1, domains[i]))
                    new_recipients_address[new_recipients_count++] = strdup(recipients_address[j]);
            }
            
        }
        char *new_sender_address = strdup(sender_address);
        message_list[message_idx]->ReplaceInfo(
            new_sender_address,
            new_recipients_address,
            new_recipients_count
        );
        message_list[message_idx]->GenerateInfoFile();
        //message_list[message_idx]->ReadInfoFile();
        
        free((void*)new_sender_address);
        for (int i = 0; i < new_recipients_count; i++) 
            free((void*)new_recipients_address[i]);
        delete[] new_recipients_address;

        write_log(
            "[SMTP-DAEMON] Message %s send partial done\n",
            message_list[message_idx]->GetId()
        );
    } else {
        write_log(
            "[SMTP-DAEMON] Message %s send completely done, message removed from queue\n",
            message_list[message_idx]->GetId()
        );
    }
    
    for (int i = 0; i < domains_count; i++) 
        if (domains[i])
            free((void*)domains[i]);
    delete[] domains;
    delete[] domains_done;

    return all_domains_done? 0: -1;
}



void MailQueue::HandleQueue() 
{
    time_t curtime;
    time(&curtime);
    
    bool queue_changed = 0;
    for (int i = 0; i < max_message_count; i++) {
        if (!message_list[i])
            continue;
        
        if (curtime - message_list[i]->GetCreateTime() > lifetime) {
            queue_changed = 1;
            DeleteMessageFromQueue(i);
        }
        
        if (curtime - last_send_attempt[i] > sending_delay ||
            !last_send_attempt) {
            if (!SendMessage(i)) {
                queue_changed = 1;
                DeleteMessageFromQueue(i);
            }
            else {
                last_send_attempt[i] = curtime;
            }
        }
    }
    
    if (queue_changed) {
        SaveQueue(server_options.queue_file);
    }
}



int MailQueue::GetMessageCount() const { return message_count; }

int MailQueue::GetMaxMessageCount() const { return max_message_count; }

int MailQueue::SetMaxMessageCount(int a_max_message_count) 
{
    if (a_max_message_count < message_count) 
        return -1;
    
    Message **new_message_list = new Message* [a_max_message_count];
    memset(new_message_list, 0, a_max_message_count * sizeof(Message*));
    
    for (int i = 0, p = 0; i < max_message_count; i++) {
        if (message_list[i])
            new_message_list[p++] = message_list[i];
    }
    
    delete[] message_list;
    
    message_list = new_message_list;
    max_message_count = a_max_message_count;
    
    return 0;
}

void MailQueue::SetMaxMessageLifeTime(time_t a_lifetime) 
{
    lifetime = a_lifetime;
}

void MailQueue::SetSendingDelay(time_t a_sending_delay) 
{
    sending_delay = a_sending_delay;
}



int MailQueue::LoadQueue(const char *filename) 
{    
    FILE *f = fopen(filename, "r");
    if (f == 0) {
        write_log(
            "[SMTP-DAEMON] Can't open %s file to load mailqueue\n", 
            filename
        );
        return -1;
    }
    
    fscanf(f, "%d\n", &message_count);
    
    char *id, *info_path, *data_path;
    time_t create_time;
    
    for (int i = 0; i < message_count; i++) {
        int len;
        
        id = Message::ReadLineFromFile(f, len);
        info_path = Message::ReadLineFromFile(f, len);
        data_path = Message::ReadLineFromFile(f, len);
        fscanf(f, "%ld\n", &create_time);
        
        message_list[i] = new Message(id, info_path, data_path, create_time);
        
        free((void*)id);
        free((void*)info_path);
        free((void*)data_path);
    }
    
    fclose(f);
    
    return 0;
}

int MailQueue::SaveQueue(const char *filename) const 
{
    FILE *f = fopen(filename, "w");
    if (f == 0) {
        write_log(
            "[SMTP-DAEMON] Can't open %s file to save mailqueue",
            filename
        );

        return -1;
    }
    
    fprintf(f, "%d\n", message_count);
    for (int i = 0; i < max_message_count; i++) {
        if (message_list[i] != 0) {
            fprintf(
                f, 
                "%s\n%s\n%s\n%ld\n", 
                message_list[i]->GetId(), 
                message_list[i]->GetInfoPath(), 
                message_list[i]->GetDataPath(), 
                (long int)message_list[i]->GetCreateTime()
            );
            
        }
    }
    fclose(f);

    return 0;
}



char** MailQueue::GetDomains(
    char **recipients_address, 
    int recipients_count,
    int *domains_count
)
{
    char **domains = new char* [recipients_count];
    *domains_count = 0;
    
    for (int i = 0; i < recipients_count; i++) {
        int atpos = Message::FindAtSymbolInAddress(recipients_address[i]);
        
        bool is_new_domain = 1;
        for (int j = 0; j < *domains_count; j++) {
            if (!strcmp(domains[j], recipients_address[i] + atpos + 1)) {
                is_new_domain = 0;
                break;
            }
        }
        
        if (is_new_domain)
            domains[(*domains_count)++] = strdup(recipients_address[i] + atpos + 1);
    }
    
    return domains;
}

/*
int MailQueue::GetMXRecords(const char *name, char **mxs, int limit) 
{
    unsigned char response[NS_PACKETSZ]; 
    ns_msg handle;
    ns_rr rr;
    int mx_index, ns_index, len;
    char dispbuf[4096];

    if ((len = res_search(name, C_IN, T_MX, response, sizeof(response))) < 0) {
        // WARN: res_search failed
        return -1;
    }

    if (ns_initparse(response, len, &handle) < 0) {
        // WARN: ns_initparse failed
        return 0;
    }

    len = ns_msg_count(handle, ns_s_an);
    if (len < 0)
        return 0;

    for (
        mx_index = 0, ns_index = 0;
        mx_index < limit && ns_index < len;
        ns_index++
    ) {
        if (ns_parserr(&handle, ns_s_an, ns_index, &rr)) {
            // WARN: ns_parserr failed
            continue;
        }
        ns_sprintrr (&handle, &rr, NULL, NULL, dispbuf, sizeof (dispbuf));
        if (ns_rr_class(rr) == ns_c_in && ns_rr_type(rr) == ns_t_mx) {
            char mxname[MAXDNAME];
            dn_expand(
                ns_msg_base(handle),
                ns_msg_base(handle) + ns_msg_size(handle),
                ns_rr_rdata(rr) + NS_INT16SZ, 
                mxname, 
                sizeof(mxname)
            );
            mxs[mx_index++] = strdup(mxname);
        }
    }
    return mx_index;
}
*/

//ADD CONST!!!!!!!!!!!!!!!!!!!!!
in_addr* MailQueue::GetIPAddress(const char *name)
{
    //hostent *h = new hostent;
    hostent *h = gethostbyname(name);
    
    in_addr **addr_list = (in_addr**)(h->h_addr_list);
    
    return addr_list[0];
}

void MailQueue::DisconnectFromServer(int sock_fd) 
{
    shutdown(sock_fd, 2);
    close(sock_fd);
}

int MailQueue::ConnectToServer(in_addr *server_address) 
{
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0)
        return -1;
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(25);
    addr.sin_addr = *server_address;
    
    if (0 != connect(sock_fd, (sockaddr*) &addr, sizeof(addr))) 
        return -1;
    
    return sock_fd;
}



int MailQueue::SendInfo(
    int sock_fd,
    const char *domain, const char *c_domain,
    const char *sender_address,
    char **recipients_address, int recipients_count
) 
{ 
    if (WriteCommandToSocket(sock_fd, "HELO", domain) < 0)
        return -1;
    
    
    if (WriteAddressToSocket(sock_fd, "MAIL FROM:", sender_address) < 0)
        return -1;
    
    for (int j = 0; j < recipients_count; j++) {
        int atpos = Message::FindAtSymbolInAddress(recipients_address[j]);
        if (!strcmp(recipients_address[j] + atpos + 1, c_domain)) {
            if (WriteAddressToSocket(sock_fd, "RCPT TO:", recipients_address[j]) < 0)
                return -1;
        }
    }
    return 0;
}

int MailQueue::SendData(int sock_fd, const char *message_id, InoutBuffer *data) 
{
    if (WriteCommandToSocket(sock_fd, "DATA", 0) < 0)
        return -1;
    
    /*
    time_t cur_time;
    time(&cur_time);
    const char *date = ctime(&cur_time);
    
    WriteToSocket(sock_fd, "Date: ", 6);
    WriteToSocket(sock_fd, date, strlen(date) - 1);
    //WriteToSocket(sock_fd, "\r\nMessage-ID: ", 14);
    WriteToSocket(sock_fd, "\nMessage-ID: ", 13);
    WriteToSocket(sock_fd, message_id, strlen(message_id));
    //WriteToSocket(sock_fd, "\r\n", 2);
    WriteToSocket(sock_fd, "\n", 1);
    */
    /*
    InoutBuffer curline;
    while(data->ReadLine(curline)) {
        WriteToSocket(sock_fd, curline.GetBuffer(), curline.Length());
        //write_log("[SMTP-DAEMON] curline = (%s)\n", curline.GetBuffer());
        //WriteToSocket(sock_fd, "\r\n", 2);
        WriteToSocket(sock_fd, "\n", 1);
    }
    */
    WriteToSocket(sock_fd, data->GetBuffer(), data->Length());

    WriteToSocket(sock_fd, "\r\n.\r\n", 5);
    
    return GetReply(sock_fd) < 0 ? -1: 0;
}

void MailQueue::WriteToSocket(int sock_fd, const char *msg, int msg_size) 
{ 
    write(sock_fd, msg, msg_size);
}

int MailQueue::WriteCommandToSocket(
    int sock_fd,
    const char *comm, const char *msg
) 
{
    write(sock_fd, comm, strlen(comm));
    if (msg) {
        write(sock_fd, " ", 1);
        write(sock_fd, msg, strlen(msg));
    }
    write(sock_fd, "\r\n", 2);
    
    return GetReply(sock_fd);
}

int MailQueue::WriteAddressToSocket(
    int sock_fd, 
    const char *comm, const char *addr
) 
{
    write(sock_fd, comm, strlen(comm));
    write(sock_fd, " <", 2);
    write(sock_fd, addr, strlen(addr));
    write(sock_fd, ">\r\n", 3);
    
    return GetReply(sock_fd);
}

int MailQueue::GetReply(int sock_fd) 
{ 
    int len, result = 0;
    char *data = new char [512];

    len = read(sock_fd, data, 511);
    data[len] = '\0';

    if (len > 0)
        result = (data[0] == '2') || (data[0] == '3') ? 0: -1;
    
    delete[] data;
    
    return result;
}





