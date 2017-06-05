#ifndef DAEMON_H_SENTRY
#define DAEMON_H_SENTRY

#define PID_FILE_PATH "/var/run/smtp-server.pid"

enum {
    CHILD_NEED_TERMINATE = 0,
    CHILD_NEED_WORK = 1
};

extern int (*init_func)();
extern void (*main_func)();
extern void (*fin_func)();

int start_daemon(int argc, char **argv);

void write_log(const char *format, ...);

#endif