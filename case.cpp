#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

#include "case.h"
#include "constants.h"

#define ESC 0x1B
#define RED 31
#define GREEN 32
#define WHITE 37
#define BRIGHT 1

/* List all available cases */
int list_cases(char *output) {
    int index = 0;
    struct dirent *this_case;
    DIR *cases = opendir("./cases/");
    
    index += sprintf(output + index, "Available cases:\n");
    index += sprintf(output + index, "----------------\n");
    while(this_case = readdir(cases)) { /* Cases are represented by directories */
        if(this_case->d_type == DT_DIR
          && strncmp(this_case->d_name, ".", 2)
          && strncmp(this_case->d_name, "..", 3)) {
            index += sprintf(output + index, "%s\n", this_case->d_name);
        }
    }
    closedir(cases);
    return index;
}

/* List files available for a case */
int list_files(Case the_case, char* output) {
    struct dirent *this_file;
    DIR *cases = opendir("./cases/");
    int index = 0;
    index += sprintf(output + index, "Case files:\n");
    index += sprintf(output + index, "-----------\n");
    
    rewinddir(the_case.case_directory);
    while(this_file = readdir(the_case.case_directory)) {
        if(strncmp(this_file->d_name, ".", 2)
          && strncmp(this_file->d_name, "..", 3)) {
            index += sprintf(output + index, "%s\n", this_file->d_name);
        }
    }
    return index;
}

/* Process a file string to determine the location in the file system */
/* The file string can be relitive, absolute, or a case filename */
void file_string(Case the_case, char *string) {
    int i = 0;
    char buffer[BUFFER_SIZE] = { '\0' };
    /* If there are no slashes, then this must be a case file */
    while(string[i] != '\0' && i < BUFFER_SIZE) {
        if(string[i] == '/') {
            return; /* The string is either relitive or absolute */
        }
        i++;
    }
    
    /* Otherwise, this string is for a case file, and needs to be updated */
    sprintf(buffer, "%s", string);
    sprintf(string, "./cases/%s/%s", the_case.case_name, buffer);
    return;
}


/* Create a new case and open it */
Case new_case(char *input, char *output) {
    char buffer[BUFFER_SIZE] = { '\0' };
    sprintf(buffer, "./cases/%s/", input);
    mkdir(buffer, S_IRWXU | S_IRWXG);

    return open_case(input, output);
}

/* Open a case. Returns the case structure */
Case open_case(char *name, char *output) {
    Case the_case = { NULL, NULL };
    char buffer[BUFFER_SIZE] = { '\0' };

    sprintf(buffer, "./cases/%s", name);
    sprintf(the_case.case_name, name);

    the_case.case_directory = opendir(buffer);
    if(!the_case.case_directory) {
        sprintf(output, "Error: Case %s not found.\n", name);
        return the_case;
    }
    
    sprintf(buffer, "./cases/%s/log.txt", name);

    the_case.log = fopen(buffer, "a+");
    if(!the_case.log) {
        fprintf(stderr, "Error: Log file not accessable.\n");
        return the_case;
    }

    log_text(the_case, "Case opened.");
    sprintf(output, "Case \"%s\" opened. \n", name); /* Last accessed info? */
    return the_case;
}

/* Close a case. Returns an empty case structure */
Case close_case(Case c, char *output) {
    log_text(c, "Closing case.\n");
    closedir(c.case_directory);
    c.case_directory = NULL;
    fclose(c.log);
    c.log = NULL;
    sprintf(output, "Case closed.\n");
    return c;
}

/* Log a text string to the case */
void log_text(Case c, const char *text) { /* Text is assumed to end with a null terminator */
    /* First, we need a timestamp */
    time_t theTime = time(NULL);
    struct tm *local_time = localtime(&theTime);
    fprintf(c.log, "[%02i-%02i-%04i %02i:%02i:%02i] ", local_time->tm_mday,
                                                    local_time->tm_mon + 1,
                                                    local_time->tm_year + 1900,
                                                    local_time->tm_hour, 
                                                    local_time->tm_min, 
                                                    local_time->tm_sec);
    fprintf(c.log, "%s\n", text);
}

/* Show the current log file */
int show_log(Case the_case, char *output) {
    char c = '\0';
    int index = 0;
    rewind(the_case.log);
    while((c = fgetc(the_case.log)) != EOF) {
        output[index++] = c;
    }   
    return index;
}
