#include "userlist.h"

#include "iniparser/iniparser.h"
#include "daemon.h"
#include "options.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

//--------------------
UserListElem::UserListElem(
    const char *an_address,
    const char *a_password, 
    const char *a_redirect_path) 
{
    address = strdup(an_address);
    password = strdup(a_password);
    redirect_path = a_redirect_path ? strdup(a_redirect_path): 0;
    
    const char *mail_prefix = server_options.mail_dir;
    int mail_prefix_len = strlen(mail_prefix);

    int address_len = strlen(address);
    path_to_mail = new char [mail_prefix_len + address_len + 1];
    path_to_mail[0] = '\0';
    strncat(path_to_mail, mail_prefix, mail_prefix_len);
    strncat(path_to_mail, address, address_len);
    //strncat(path_to_mail, ".txt", 4);
    path_to_mail[mail_prefix_len + address_len] = '\0';

    next = prev = 0;
}

UserListElem::~UserListElem() 
{
    if (address)
        free((void*)address);
    if (password)
        free((void*)password);
    if (redirect_path)
        free((void*)redirect_path);
    
    if (path_to_mail)
        delete [] path_to_mail;
}
//--------------------


//--------------------
UserList::UserList() 
{
    head = last = 0;
    size = 0;
}

UserList::~UserList()
{
    if (size != 0) {
        UserListElem *elem = head;
        while (elem != 0) {
            UserListElem *next_elem = elem->next;
            delete elem;
            elem = next_elem;
        }
    }
}
//-----
int UserList::AddElement(
    const char *an_address,
    const char *a_password, 
    const char *a_redirect_path) 
{
    if (FindUserByAddress(an_address) >= 0)
        return -1;
    
    if (!size) {
        head = last = new UserListElem(an_address, a_password, a_redirect_path);
    } else {
        last->next = new UserListElem(an_address, a_password, a_redirect_path);
        last->next->prev = last;
        last = last->next;
    }
    return ++size;
}


const char* UserList::GetAddress(int idx) const 
{
    if ((idx < 0) || (idx >= size))
        return 0;
    
    return GetUserListElem(idx)->address;
}

const char* UserList::GetPassword(int idx) const 
{
    if ((idx < 0) || (idx >= size))
        return 0;
    
    return GetUserListElem(idx)->password;
}

const char* UserList::GetRedirectPath(int idx) const 
{
    if ((idx < 0) || (idx >= size))
        return 0;
    
    return GetUserListElem(idx)->redirect_path;
}

const char* UserList::GetPathToMail(int idx) const 
{
    if ((idx < 0) || (idx >= size))
        return 0;
    
    return GetUserListElem(idx)->path_to_mail;
}

bool UserList::IsEmpty() const { return !size; }

int UserList::GetSize() const { return size; }

int UserList::FindUserByAddress(const char *address) const 
{
    int idx = 0;
    for (UserListElem *cur = head; cur; cur = cur->next, idx++) {
        if (!strcmp(cur->address, address))
            return idx;
    }
    return -1;
}

bool UserList::VerifyPassword(int idx, const char *password) const 
{
    const UserListElem *elem = GetUserListElem(idx);
    
    if (!elem) 
        return 0;
    
    return !strcmp(elem->password, password);
}

int UserList::Load(const char *users_file, const char *params_file) 
{
    FILE *f_users = fopen(users_file, "r");
    if (f_users == NULL) {
        write_log(
            "[SMTP-DAEMON] Can't open %s file to load userlist\n",
            users_file
        );
        return -1;
    }

    dictionary *dict = NULL;
    dict = iniparser_load(params_file);
    if (dict == NULL) {
        write_log(
            "[SMTP-DAEMON] Can't open %s file to load userlist\n",
            params_file
        );
        return -1;
    }
    

    char *address;
    while ((address = ReadLineFromFile(f_users))) {
        int len = strlen(address);
        const char *password, *redirect_path;

        char *password_key = new char [len + 10];
        strcpy(password_key, address);
        strcpy(password_key + len, ":password");
        password_key[len + 9] = '\0';

        password = iniparser_getstring(
            dict, 
            password_key,
            ""
        );

        char *redirect_path_key = new char [len + 15];
        strcpy(redirect_path_key, address);
        strcpy(redirect_path_key + len, ":redirect_path");
        redirect_path_key[len + 14] = '\0';

        redirect_path = iniparser_getstring(
            dict, 
            redirect_path_key,
            "0"
        );


        if (redirect_path[0] == '0' && redirect_path[1] == '\0') {
            write_log("[SMTP-DAEMON] USERLIST: (%s) redirect_path = 0\n", address);
            AddElement(address, password);
        }
        else 
            AddElement(address, password, redirect_path);

        write_log("[SMTP-DAEMON] USERLIST: ((%s) (%s) (%s))\n", address, password, redirect_path);

        delete [] password_key;
        delete [] redirect_path_key;
        delete [] address;
    }

    write_log("[SMTP-DAEMON] USERLIST: done\n");

    fclose(f_users);
    if (dict)
        iniparser_freedict(dict);

    return 0;
}

int UserList::Save(const char *users_file, const char *params_file) 
{
    FILE *f_users = fopen(users_file, "w");
    if (f_users == NULL) {
        write_log(
            "[SMTP-DAEMON] Can't open %s file to save userlist\n",
            users_file
        );
        return -1;
    }
    FILE *f_params = fopen(params_file, "w");
    if (f_params == NULL) {
        write_log(
            "[SMTP-DAEMON] Can't open %s file to save userlist\n",
            params_file
        );
        return -1;
    }
    
    for (UserListElem *cur = head; cur; cur = cur->next) {
        fprintf(f_users, "%s\n", cur->address);
        fprintf(f_params, "[%s]\n", cur->address);
        fprintf(f_params, "password      = %s\n", cur->password);
        fprintf(f_params, "redirect_path = %s\n", cur->redirect_path);
    }
    
    fclose(f_users);
    fclose(f_params);

    return 0;
}

/*
int UserList::Load(const char *filename) 
{
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        write_log(
            "[SMTP-DAEMON] Can't open %s file to load userlist\n",
            filename
        );
        return -1;
    }
    
    char *password, *address, *redirect_path;
    while ((address = ReadLineFromFile(f))) {
        password = ReadLineFromFile(f);
        redirect_path = ReadLineFromFile(f);

        if (redirect_path[0] == '0' && redirect_path[1] == '\0')
            AddElement(address, password);
        else 
            AddElement(address, password, redirect_path);
    

        write_log(
            "[SMTP-DAEMON] USERLIST: (%s)\n", 
            address
        );

        delete[] password;
        delete[] address;
        delete[] redirect_path;
    }

    fclose(f);

    return 0;
}
*/


/*
int UserList::Save(const char *filename) 
{
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        write_log(
            "[SMTP-DAEMON] Can't open %s file to save userlist\n",
            filename
        );
        return -1;
    }
    
    for (UserListElem *cur = head; cur; cur = cur->next)
        fprintf(
            f, 
            "%s\n%s\n%s\n", 
            cur->address, 
            cur->password, 
            cur->redirect_path
        );
    
    fclose(f);

    return 0;
}
*/
//-----
const UserListElem* UserList::GetUserListElem(int idx) const
{
    if ((idx < 0) || (idx >= size))
        return 0;
    
    UserListElem *elem = head;
    for (int i = 0; i < idx; i++, elem = elem->next);
    
    return elem;
}


char* UserList::IncreaseBuffer(char *buf, int len, int &max_len) { 
    max_len *= 2;
    
    char *new_buf = new char [max_len];
    for (int i = 0; i < len; i++)
        new_buf[i] = buf[i];
    
    delete [] buf;
    
    return new_buf;
}

char* UserList::ReadLineFromFile(FILE *f) {
    int len = 0, max_len = 256;
    char c, *result = new char [max_len];
    
    while (fscanf(f, "%c", &c) > 0) {
        if (len + 1 >= max_len) 
            result = IncreaseBuffer(result, len, max_len);
            
        if (c == '\n') 
            break;
        
        result[len++] = c;
    }
    
    result[len] = '\0';
    
    if (len == 0) {
        delete [] result;
        result = 0;
    }
    
    return result;
}
//--------------------


