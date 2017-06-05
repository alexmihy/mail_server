#ifndef OPTIONS_H_SENTRY
#define OPTIONS_H_SENTRY

#include "iniparser/iniparser.h"

class Options 
{
    dictionary *dict;

public:
    int smtp_port;
    const char *domain;
    int timeout;
    int max_connections;

    int max_recipients;
    int max_message_size;
    const char *mail_dir;
    
    const char *queue_dir;
    const char *queue_file;
    int max_messages;

    const char *init_whitelist_file;
    const char *whitelist_file;
    const char *graylist_file;
    const char *blacklist_file;
    int min_time_to_pass;
    int max_time_to_pass;
    int max_time_to_store;

    const char *users_file;
    const char *users_params_file;

    Options();
    int Load(const char *filename);
    ~Options();
};

extern Options server_options;

#endif