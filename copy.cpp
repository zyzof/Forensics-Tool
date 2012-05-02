/* Copy a file to the case directory */
/* *original is the full path to the original file */
/* *copy is the filename of the new file, to be created in the case folder */
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "case.h"
#include "copy.h"

int copy(Case the_case, char *original, char *copy, char *output) {
    FILE *o = NULL;
    FILE *c = NULL;
    unsigned long long int bytes = 0;
    int byte = '\0';
    char buffer[161]; /* Assuming a shortish file path */
    int index = 0;

    sprintf(buffer, "./cases/%s/%s", the_case.case_name, copy);

    /* First, check that a file does not already exist with the same name as the copy */
    c = fopen(buffer, "r");
    if(c) {
        fclose(c);
        sprintf(buffer, "Warning: Attempt to overwrite %s with %s.", copy, original);
        log_text(the_case, buffer);
        index += sprintf(output + index, "Error: A file already exists with the name %s. Overwriting forensic data is not allowed.\n", copy);
        return index;
    }
    
    o = fopen(original, "r");
    if(!o) {
        index += sprintf(output + index, "Error: Can not open %s\n", original);
        return index;
    }
    c = fopen(buffer, "w"); /* We should only create this file if it does not already exist. Otherwise, report an error. */
    if(!c) {
        index += sprintf(output + index, "Error: Can not open %s\n", copy);
        return index;
    }

    byte = fgetc(o);
    while(byte != EOF) {
        fputc(byte, c);
        byte = fgetc(o);
        bytes++;
    }
   
    fclose(o); /* Close original */
    fclose(c); /* CLose copy */

    sprintf(buffer, "Copied %s to case directory as %s. %llu bytes.", original, copy, bytes);
    log_text(the_case, buffer);
     
    index += sprintf(output + index, "%llu bytes copied.\n", bytes);
    return index;
}
