#ifndef HEADER_SENTRY_H
#define HEADER_SENTRY_H

#include "buffer.h"

class HeaderParser
{
    enum {
        K_MAX_NAME_SIZE     = 128,
        K_MAX_VALUE_SIZE    = 2048,
        K_MAX_HEADERS_COUNT = 64
    };

    struct MailHeaders{
        char name[K_MAX_NAME_SIZE];
        char value[K_MAX_VALUE_SIZE];
    };

    int header_count;
    MailHeaders headers[K_MAX_HEADERS_COUNT];

    InoutBuffer mail_header, mail_body;

public:
    HeaderParser();
    ~HeaderParser();

    const char* GetField(const char *name) const;
    int SetField(const char *name, const char *value);
    int AddField(const char *name, const char *value);

    void ReadMail(const char *mail, int mail_len);
    void GenerateHeader();

    const InoutBuffer& GetBody() const;
    const InoutBuffer& GetHeader() const;
};

extern HeaderParser header_parser;

#endif