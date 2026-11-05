// Kian OS - userspace init (pid 1)
// Compiled with x86_64-kian cross-gcc, linked against kian libc (planned milestone).
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("[Kian] init (pid 1) starting...\n");
    printf("[Kian] Welcome to Kian OS 0.1.0 \"Genesis\"\n");

    // Ignore SIGINT/SIGQUIT in init
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

    // Spawn login manager
    while (1) {
        pid_t pid = fork();
        if (pid == 0) {
            // child: run login manager
            execv("/bin/login", argv);
            perror("execv /bin/login");
            _exit(1);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            printf("[Kian] login exited (%d), respawning...\n", status);
            sleep(1);
        } else {
            perror("fork");
            sleep(1);
        }
    }
    return 0;
}
