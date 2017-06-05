#include "resolve.h"
#include "buffer.h"
#include "daemon.h"

#include <stdio.h>
#include <string.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h>
#include <unistd.h> 
#include <ctype.h>
#include <errno.h>


DNSMXResolver::DNSMXResolver() 
{
    dns_server_count = 0;
}

DNSMXResolver::~DNSMXResolver() { }




int DNSMXResolver::Init() 
{
    return GetDNSServers();
}

char* DNSMXResolver::GetMXWithLowestPreference(const char *host)
{
    int s = socket(AF_INET , SOCK_DGRAM , IPPROTO_UDP);
    if (s < 0) {
        write_log(
            "[SMTP-DAEMON] DNSMXResolver could not open socket: (%s)", 
            strerror(errno)
        );
        return 0;
    }
 
    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(53);
    dest.sin_addr.s_addr = inet_addr(dns_servers[0]);

    unsigned char buf[K_BUF_SIZE];
    struct DNS_HEADER *dns = NULL;
    dns = (struct DNS_HEADER *)&buf;
 
    dns->id = (unsigned short) htons(getpid());
    dns->qr = 0; //This is a query
    dns->opcode = 0; //This is a standard query
    dns->aa = 0; //Not Authoritative
    dns->tc = 0; //This message is not truncated
    dns->rd = 1; //Recursion Desired
    dns->ra = 0; //Recursion not available!
    dns->z = 0;
    dns->ad = 0;
    dns->cd = 0;
    dns->rcode = 0;
    dns->q_count = htons(1); //we have only 1 question
    dns->ans_count = 0;
    dns->auth_count = 0;
    dns->add_count = 0;


    unsigned char *_host = new unsigned char [strlen(host) + 1];
    strcpy((char*)_host, host);

    unsigned char *qname;
    qname = (unsigned char*)&buf[sizeof(struct DNS_HEADER)];
    ChangetoDNSNameFormat(qname, _host);
    delete [] _host;

    struct QUESTION *qinfo = NULL;
    qinfo = (struct QUESTION*)&buf[sizeof(struct DNS_HEADER) + (strlen((const char*)qname) + 1)]; //fill it
 
    qinfo->qtype = htons(T_MX); //type of the query , A , MX , CNAME , NS etc
    qinfo->qclass = htons(1); //its internet

    if (sendto(
            s, 
            (char*)buf, 
            sizeof(struct DNS_HEADER) + (strlen((const char*)qname) + 1) + sizeof(struct QUESTION), 
            0, 
            (struct sockaddr*)&dest, 
            sizeof(dest)
        ) < 0) {
        write_log(
            "[SMTP-DAEMON] DNSMXResolver sendto error: (%s)\n", 
            strerror(errno)
        );
        return 0;
    }

    int i = sizeof dest;
    if (recvfrom(
            s, 
            (char*)buf, 
            K_BUF_SIZE, 
            0, 
            (struct sockaddr*)&dest, 
            (socklen_t*)&i
        ) < 0) {
        write_log(
            "[SMTP-DAEMON] DNSMXResolver recvfrom error: (%s)\n", 
            strerror(errno)
        );
        return 0;
    }

    dns = (struct DNS_HEADER*) buf;

    unsigned char *reader;
    reader = &buf[sizeof(struct DNS_HEADER) + (strlen((const char*)qname) + 1) + sizeof(struct QUESTION)]; 

    struct RES_RECORD answers[K_MAX_ANSWER_SIZE];
    int stop = 0, preference[K_MAX_ANSWER_SIZE], min_preference_idx = -1;
    for(i = 0; i < ntohs(dns->ans_count); i++)
    {
        answers[i].name = ReadName(reader, buf, stop);
        reader = reader + stop;
 
        answers[i].resource = (struct R_DATA*)(reader);
        reader = reader + sizeof(struct R_DATA);

        preference[i] = (*reader) * 256 + *(reader + 1);
        answers[i].rdata = (char*)ReadName(reader + 2, buf, stop);

        reader = reader + ntohs(answers[i].resource->data_len);

        if (min_preference_idx == -1 || 
            preference[i] ) {
            min_preference_idx = i;
        }
    }

    return answers[min_preference_idx].rdata;
}




void DNSMXResolver::ChangetoDNSNameFormat(
    unsigned char *dns, 
    unsigned char *host)
{
    int lock = 0 , i;

    strcat((char*)host, ".");

    for(i = 0 ; i < (int)strlen((const char*)host); i++) {
        if(host[i] == '.') {
            *dns++ = i - lock;
            for(; lock < i; lock++) {
                *dns++=host[lock];
            }
            lock++;
        }
    }
    *dns++='\0';
}

int DNSMXResolver::GetDNSServers() 
{
    FILE *fp;
    
    if((fp = fopen("/etc/resolv.conf" , "r")) == NULL) {
        //printf("Failed opening /etc/resolv.conf file\n");
        write_log("[SMTP-DAEMON] Failed opening /etc/resolv.conf file\n");
        return -1;
    }

    char c; 
    InoutBuffer buf, curline;
    while (fscanf(fp, "%c", &c) > 0) {
        buf.AddChar(c);
    }

    char *p, *line;
    while (buf.ReadLine(curline)) {
        int len = curline.Length();

        line = new char [len + 1];
        curline.GetData(line, len);
        line[len] = '\0';
        
        if (line[0] == '#')
            continue;

        if (!strncmp(line, "nameserver", 10)) {
            p = strtok(line, " ");
            p = strtok(NULL, " ");

            p[strlen(p)] =  '\0';

            strcpy(dns_servers[dns_server_count++], p);

            if (dns_server_count >= K_MAX_DNS_SERVER_COUNT) {
                delete [] line;
                break;
            }
        }
        delete [] line;
    }

    /*
    for (int i = 0; i < dns_server_count; i++) {
        write_log("DNS server = (%s)\n", dns_servers[i]);
    }
    */

    return 0;
}

unsigned char* DNSMXResolver::ReadName(
    unsigned char* reader,
    unsigned char* buffer,
    int &count)
{
    unsigned char *name;
    
    count = 1;
    name = new unsigned char [256];
    name[0] = '\0';

    unsigned int p = 0, jumped = 0, offset;
    while (*reader != 0) {
        if (*reader >= 192) {
            offset = (*reader) * 256 + *(reader + 1) - 49152; 
            reader = buffer + offset - 1;
            jumped = 1;
        } else {
            name[p++] = *reader;
        }

        reader = reader + 1;

        if (jumped == 0) 
            count = count + 1;
    }

    name[p] = '\0';
    if (jumped == 1) {
        count = count + 1;
    }

    int i, j;
    for (i = 0; i < (int)strlen((const char*)name); i++) {
        p = name[i];
        for (j = 0; j < (int)p; j++) {
            name[i] = name[i + 1];
            i++;
        }
        name[i]='.';
    }
    
    name[i - 1] = '\0';
    
    return name;


}