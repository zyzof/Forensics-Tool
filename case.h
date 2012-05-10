#include <dirent.h>

/* A structure to represent an open case */
typedef struct Case_struct {
    DIR *case_directory; /* An open directory of case files */
    FILE *log; /* The log file. Open for reading and appending */
    char case_name[81]; /* Assuming a friendly user... */
    int local; /* Are we local (treaded as boolean) */
    int socket; /* File descriptor to the socket, if we are remote */
    void* snifferState;
} Case;

/* List all cases in the system */
int list_cases(char *output);

/* List all files related to a case */
int list_files(Case the_case, char *output);

/* Modify a string so that it can be used with fopen() and friends. */
void  file_string(Case the_case, char *string);

/* Create a new case and open it */
Case new_case(char *input, char *output, Case the_case);

/* Open a case */
Case open_case(char *name, char *output, Case the_case);

/* Close the current case */
Case close_case(Case c, char *output);

/* Log a text string to the case */
void log_text(Case c, const char *text);

void log_text_without_timestamp(Case c, const char *text);

/* Display the case log */
int show_log(Case c, char *output);
