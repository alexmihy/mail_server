#include "header.h"
#include "daemon.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static inline int min(int a, int b) 
{
    return a < b ? a : b;
}


HeaderParser::HeaderParser() 
{
    header_count = 0;
    mail_body.DropAll();
    mail_header.DropAll();
    memset(&headers, '\0', sizeof(MailHeaders));
}

HeaderParser::~HeaderParser() { }

const char* HeaderParser::GetField(const char *name) const 
{
    const char *value = NULL;

    for(int i = 0; i < header_count; i++) {
        if(!strcmp(name, headers[i].name)) {
            value = headers[i].value;
            break;
        }
    }

    return (value);
}

int HeaderParser::SetField(const char *name, const char *value) 
{
    int index = -1;

    for (int i = 0; i < header_count; i++) {
        if (!strcmp(name, headers[i].name)) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        if (header_count == K_MAX_HEADERS_COUNT)
            return -1;

        index = header_count;
        strncpy(headers[index].name, name, min(K_MAX_NAME_SIZE, strlen(name) + 1));
        strncpy(headers[index].value, value, min(K_MAX_VALUE_SIZE, strlen(value) + 1));

        header_count++;
    } else {
        strncpy(headers[index].value, value, min(K_MAX_VALUE_SIZE, strlen(value) + 1));
    }

    if (headers[index].value[strlen(value) - 1] == '\r' ||
        headers[index].value[strlen(value) - 1] == '\n')
        headers[index].value[strlen(value) - 1] = '\0';

    return index;
}

int HeaderParser::AddField(const char *name, const char *value) 
{
    if (header_count == K_MAX_HEADERS_COUNT)
        return -1;

    int index = header_count;

    strncpy(headers[index].name, name, min(K_MAX_NAME_SIZE, strlen(name) + 1));
    strncpy(headers[index].value, value, min(K_MAX_VALUE_SIZE, strlen(value) + 1));

    header_count++;

    return index;
}

void HeaderParser::ReadMail(const char *mail, int mail_len) 
{
    header_count = 0;
    mail_body.DropAll();
    mail_header.DropAll();
    memset(&headers, '\0', sizeof(MailHeaders));

    InoutBuffer buf, curline;
    buf.AddData(mail, mail_len);

    int index = 0;
    char *line = NULL, *name = NULL, *value = NULL;
    while (buf.ReadLine(curline)) {
        line = strdup(curline.GetBuffer());        
        
        if (*line == '\t') {
            strcat(headers[index].value, line); 
        } else {
            name = line;  
            value = strchr(line, ':'); 

            if (value != NULL) {
                *value = '\0';
                value += 2;
                index = SetField(name, value);
            }

        }

        if (*line == '\0') {
            while (buf.ReadLine(curline)) {
                mail_body.AddString(curline.GetBuffer());
                mail_body.AddChar('\n');
            }

            break;
        }

        free((void*)line);
    }
}

void HeaderParser::GenerateHeader() 
{
    for (int i = 0; i < header_count; i++) {
        write_log("[SMTP-DAEMON] HEADERS = (%s): (%s)\n", headers[i].name, headers[i].value);
    }

    mail_header.DropAll();

    for (int i = 0; i < header_count; i++) {
        mail_header.AddString(headers[i].name);
        mail_header.AddString(": ");
        mail_header.AddString(headers[i].value);
        mail_header.AddString("\r\n");
    }
    mail_header.AddString("\n");
}


const InoutBuffer& HeaderParser::GetBody() const 
{
    return mail_body;
}

const InoutBuffer& HeaderParser::GetHeader() const 
{
    return mail_header;
}

