#include <cstdio>
#include <ifstream>

#include <unistd.h>
#include <sys/types.h>

#include "navigator.h"

void run_file_navigator() {
    pid_t child = -1;
    ifstream ifile("./filenav");
    if (ifile) {
        child = fork();
    }
    if (child == -1) {
        printf("Failed to execute file navigator.\n");
    }
    else if (child == 0) {
        execl("./filenav", "./filenav", (char *) NULL);
    }
    else {
        printf("Loading file navigator...\n");
    }
}