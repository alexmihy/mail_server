#include "options.h"
#include "daemon.h"

#include <string.h>

Options::Options() 
{
    dict = NULL;
}

int Options::Load(const char *filename)
{
    if (dict)
        iniparser_freedict(dict);

    dict = iniparser_load(filename);

    smtp_port = iniparser_getint(dict, "server:smtp_port", 25);
    domain = iniparser_getstring(dict, "server:domain", "");
    if(!strncasecmp(domain, "www.", 4))
        domain += 4;
    timeout = iniparser_getint(dict, "server:timeout", 600);
    max_connections = iniparser_getint(dict, "server:max_connections", 20);

    //write_log("%d (%s) %d %d\n", smtp_port, domain, timeout, max_connections);

    max_recipients = iniparser_getint(dict, "smtp:max_recipients", 20);
    max_message_size = iniparser_getint(dict, "smtp:max_message_size", 30000);
    mail_dir = iniparser_getstring(dict, "smtp:mail_dir", "");

    //write_log("%d %d (%s)\n", max_recipients, max_message_size, mail_dir);

    queue_dir = iniparser_getstring(dict, "queue:queue_dir", "");
    queue_file = iniparser_getstring(dict, "queue:queue_file", "");
    max_messages = iniparser_getint(dict, "queue:max_messages", 15);

    //write_log("(%s) (%s) %d\n", queue_dir, queue_file, max_messages);

    init_whitelist_file = iniparser_getstring(
        dict, 
        "ip_address_list:init_whitelist_file", 
        ""
    );
    whitelist_file = iniparser_getstring(
        dict, 
        "ip_address_list:whitelist_file", 
        ""
    );
    graylist_file = iniparser_getstring(
        dict, 
        "ip_address_list:graylist_file", 
        ""
    );
    blacklist_file = iniparser_getstring(
        dict, 
        "ip_address_list:blacklist_file", 
        ""
    );
    min_time_to_pass = iniparser_getint(
        dict, 
        "ip_address_list:min_time_to_pass", 
        120
    );
    max_time_to_pass = iniparser_getint(
        dict, 
        "ip_address_list:max_time_to_pass",
        18000
    );
    max_time_to_store = iniparser_getint(
        dict, 
        "ip_address_list:max_time_to_store", 
        1209600
    );

    //write_log("min = %d max = %d\n", min_time_to_pass, max_time_to_pass);
    //write_log("(%s) (%s) (%s) (%s) %d %d\n", init_whitelist_file, whitelist_file, graylist_file, blacklist_file, min_time_to_pass, max_time_to_store);

    users_file = iniparser_getstring(dict, "user_list:users_file", "");
    users_params_file = iniparser_getstring(
        dict, "user_list:users_params_file", ""
    );

    //write_log("(%s)\n", userlist_file);


    return 0;
}

Options::~Options() 
{
    if (dict)
        iniparser_freedict(dict);
}