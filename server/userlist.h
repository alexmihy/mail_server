#ifndef USERLIST_H_SENTRY
#define USERLIST_H_SENTRY

#include <stdio.h>

struct UserListElem 
{
    char *address, *password;
    char *redirect_path;
    char *path_to_mail;
    
    UserListElem *next, *prev;
    
    UserListElem(
        const char *an_address,
        const char *a_password,
        const char *a_redirect_path = 0
    );
    ~UserListElem();
    
};

class UserList 
{
    UserListElem *head, *last;

    int size;
public:
    UserList();
    ~UserList();
    
    int AddElement(
        const char *an_address,
        const char *a_password, 
        const char *a_redirect_path = 0
    );
    
    const char* GetAddress(int idx) const;
    const char* GetPassword(int idx) const;
    const char* GetRedirectPath(int idx) const;
    const char* GetPathToMail(int idx) const;
    
    int FindUserByAddress(const char *address) const;
    bool VerifyPassword(int idx, const char *password) const;
    
    bool IsEmpty() const;
    int GetSize() const;
    
    int Load(const char *users_file, const char *params_file);
    int Save(const char *users_file, const char *params_file);
private:
    const UserListElem* GetUserListElem(int idx) const;
    static char* IncreaseBuffer(char *buf, int len, int &max_len);
    static char* ReadLineFromFile(FILE *f);
};
#endif