// tss timer.c -o timer.mdl
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#define NOW "now"


char* get_current_time();
int main(int argc, char* argv[]);


char* get_current_time() {
    time_t rawtime;
    struct tm* timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    char* time_str = asctime(timeinfo);
    time_str[strlen(time_str) - 1] = '\0';

    return time_str;
}

int main(int argc, char* argv[]) {
    if (argc != 2) exit(1);
    if (strcmp(argv[1], NOW) == 0) {
        printf("%s", get_current_time());
        return 100;
    }

    return 2;
}