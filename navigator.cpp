#include <cstdio>
#include <fstream>
#include <cstdlib>

using namespace std;

#include <unistd.h>
#include <sys/types.h>

#include "navigator.h"

void run_file_navigator() {
    pid_t child = -1;
    ifstream ifile("./filenav");
    if (ifile) {
        child = fork();
    }
    else {
        printf("\nFile navigator not found.");
        return;
    }
    if (child == -1) {
        printf("\nFailed to execute file navigator.");
    }
    else if (child == 0) {
        execl("./filenav", "./filenav", (char *) NULL);
        exit(0);
    }
    else {
        printf("\nLoading file navigator...");
    }
}