// Kian OS - su utility
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    const char *target = (argc > 1) ? argv[1] : "root";
    char pass[128];
    printf("Password: ");
    fflush(stdout);
    if (!fgets(pass, sizeof(pass), stdin)) return 1;
    char *n = strchr(pass,'\n'); if(n)*n=0;
    if (strcmp(target,"root")==0 && strcmp(pass,"")==0) {
        printf("Switched to root.\n"); return 0;
    }
    if (strcmp(target,"kian")==0 && strcmp(pass,"kian")==0) {
        printf("Switched to kian.\n"); return 0;
    }
    printf("Authentication failure.\n"); return 1;
}
