#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <execinfo.h>
#include <syslog.h>
#include <stdarg.h>

#include "daemon.h"
#include "options.h"

static bool child_dead = 0;
static char *config_file = 0;

void write_log(const char *format, ...) 
{
    va_list valist;

    va_start(valist, format);

    vsyslog(LOG_WARNING, format, valist);

    va_end(valist);
}

static int load_config(const char *config_name) 
{
    if (config_file)
        free((void*)config_file);
    config_file = strdup(config_name);

    return server_options.Load(config_name);
}

static int reload_config() 
{
    return server_options.Load(config_file);
}

static void create_pid_file(const char *path)
{
    FILE *f;

    f = fopen(path, "w+");
    if (f) {
        fprintf(f, "%u\n", getpid());
        fclose(f);
    }
}

static void signal_error(int sig, siginfo_t *si, void *ptr)
{
    write_log(
        "[SMTP-DAEMON] Signal: %s, Addr: 0x%0.16X\n", 
        strsignal(sig), 
        si->si_addr
    );

    void *error_addr;
    #if __WORDSIZE == 64 
        error_addr = (void*)((ucontext_t*)ptr)->uc_mcontext.gregs[REG_RIP];
    #else
        error_addr = (void*)((ucontext_t*)ptr)->uc_mcontext.gregs[REG_EIP];
    #endif

    void *trace[16];
    int trace_size;
    trace_size = backtrace(trace, 16);
    trace[1] = error_addr;


    char **messages;
    messages = backtrace_symbols(trace, trace_size);
    if (messages)
    {
        write_log("== Backtrace ==\n");

        for (int x = 1; x < trace_size; x++) {
            write_log("%s\n", messages[x]);
        }

        write_log("== End Backtrace ==\n");
        free(messages);
    }

    write_log("[SMTP-DAEMON] Stopped\n");

    fin_func();

    exit(CHILD_NEED_WORK);
}

static void sigchld_handler(int signo)
{
    signal(signo, sigchld_handler);

    child_dead = 1;
}

static void sigterm_handler(int signo)
{
    exit(CHILD_NEED_TERMINATE);
}

static int work_process() 
{
    //openlog("SMTP-DAEMON", LOG_PID, LOG_DAEMON);

    struct sigaction sigact;

    sigact.sa_flags = SA_SIGINFO;
    sigact.sa_sigaction = signal_error;

    sigemptyset(&sigact.sa_mask);

    sigaction(SIGFPE, &sigact, 0);
    sigaction(SIGILL, &sigact, 0);
    sigaction(SIGSEGV, &sigact, 0);
    sigaction(SIGBUS, &sigact, 0);

    signal(SIGTERM, sigterm_handler);

    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = 10;

    sigset_t sigset;

    sigemptyset(&sigset);

    sigaddset(&sigset, SIGQUIT);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGUSR1);

    sigprocmask(SIG_UNBLOCK, &sigset, NULL);

    write_log("[SMTP-DAEMON] Started\n");

    if (!init_func) {
        write_log("[SMTP-DAEMON] Initialize function not implemented. Stop\n");
        return CHILD_NEED_TERMINATE;
    }
    if (!main_func) {
        write_log("[SMTP-DAEMON] Main loop function not implemented. Stop\n");
        return CHILD_NEED_TERMINATE;
    }
    if (!fin_func) {
        write_log("[SMTP-DAEMON] Finalize function not implemented. Stop\n");
        return CHILD_NEED_TERMINATE;
    }

    int status = init_func();
    if (!status) {
        for (;;) {
            main_func();

            //siginfo_t siginfo;
            //int signo = sigtimedwait(&sigset, &siginfo, &timeout);
            int signo = 0;

            if (signo == SIGUSR1) {
                status = reload_config();
                if (!status)
                    write_log("[SMTP-DAEMON] Config sucessful reloaded\n");
                else
                    write_log("[SMTP-DAEMON] Config reload failed\n");

            } else if (signo > 0) {
                break;
            }
        }
    } else {
        write_log("[SMTP-DAEMON] Initialization failed\n");
    }
    fin_func();

    write_log("[SMTP-DAEMON] Stop\n");
    closelog();

    return CHILD_NEED_TERMINATE;
}

static int monitor_process() 
{
    openlog("SMTP-MONITOR", LOG_PID, LOG_DAEMON);

    signal(SIGCHLD, sigchld_handler);

    sigset_t sigset;

    sigemptyset(&sigset);

    sigaddset(&sigset, SIGQUIT);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);
    //sigaddset(&sigset, SIGCHLD);
    sigaddset(&sigset, SIGUSR1);

    sigprocmask(SIG_BLOCK, &sigset, NULL);

    create_pid_file(PID_FILE_PATH);

    write_log("[SMTP-MONITOR] Started\n");

    int pid = 0, status;
    bool need_start = 1;
    for (;;) {
        if (need_start)
            pid = fork();

        need_start = 1;

        if (pid == -1) {
            write_log("[SMTP-MONITOR] fork() failed (%s)\n", strerror(errno));
        } else if (!pid) {
            status = work_process();
            exit(status);
        } else {
            siginfo_t siginfo;
            sigwaitinfo(&sigset, &siginfo);

            if (child_dead) {
                wait(&status);
                status = WEXITSTATUS(status);
                child_dead = 0;

                if (status == CHILD_NEED_TERMINATE) {
                    write_log("[SMTP-MONITOR] Child stopped\n");
                    break;
                } else if (status == CHILD_NEED_WORK) {
                    write_log("[SMTP-MONITOR] Child restart\n");
                }
            } else if (siginfo.si_signo == SIGUSR1) {
                kill(pid, SIGUSR1);
                need_start = 0;
            } else {
                write_log("[SMTP-MONITOR] Signal %s\n", strsignal(siginfo.si_signo));

                kill(pid, SIGTERM);
                status = 0;
                break;
            }

        }

    }

    write_log("[SMTP-MONITOR] Stop\n");
    unlink(PID_FILE_PATH);
    closelog();

    return status;
}




int start_daemon(int argc, char **argv) 
{
    //Loading config
    int status;
    if (argc != 2) {
        status = load_config("/usr/local/etc/smtp-server/server.config");
    } else {
        status = load_config(argv[1]);
    }
    
    //Handle error occured while loading config
    if (status == -1) {
        printf("Error: Load config failed\n");
        return -1;
    }

    //Creating child
    int pid = fork();

    if (pid == -1) {
        //If fork() failed
        printf("Error: Start daemon failed (%s)\n", strerror(errno));
        return -1;
    } else if (!pid) {
        //Child process
        umask(0);

        setsid();

        if (chdir("/")) {
            printf("Error: (%s)\n", strerror(errno));
            return 1;
        }

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        status = monitor_process();

        return status;
    } else {
        //Parent process
        return 0;
    }
}