// Kian OS - whoami utility
#include <stdio.h>
#include <unistd.h>
#include <security/auth.h>
int main(int argc, char **argv) { (void)argc; (void)argv; printf("%s\n", current_user_name() ? current_user_name() : "kian"); return 0; }
