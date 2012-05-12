#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <dirent.h>
//#include <stropts.h>
#include <linux/hdreg.h>
#include <linux/fs.h>

#include <pthread.h>

#include "constants.h"
#include "case.h"
#include "copy.h"
#include "disk.h"
#include "client.h"
#include "server.h"
#include "cli.h"

#define ESC 0x1B
#define RED 31
#define GREEN 32
#define WHITE 37
#define BRIGHT 1

int server_running = 0;
pthread_t the_listener;

int case_manager() {
    int running = 1;
    int case_open = 0;
    int disk_fd = 0;
    Case current_case = { NULL, NULL, { '\0' }, 1, 0, NULL };
    char input_buffer[BUFFER_SIZE] = { '\0' }; 
    char buffer_B[BUFFER_SIZE] = { '\0' };
    char output_buffer[BUFFER_SIZE] = { '\0' };
    /* Should we chane the current working directory to the program directory first? */
   
    the_listener = 0; /* Remote access listener thread */

    DIR *cases = opendir("./cases/");
    if(!cases) { /* Directory does not exist yet */
        mkdir("./cases/", S_IRWXU | S_IRWXG | S_IRWXO); /* So we better create it */
    }
    closedir(cases);

    int index = 0; /* Index to the current position of the output buffer */

    while(running) {
        title();
        main_menu_help();
        current_case.local = 1;
        printf("\nlocal > "); /* Command prompt */

        while(1) {
            index = 0;
            gets(input_buffer);

            index += sprintf(output_buffer, "\n");

            /* exit */
            if(!strncmp(input_buffer, "exit", 5)) {
    
                if(case_open) {
                    current_case = close_case(current_case, output_buffer + index);
                }

                running = 0;
                break;
            }

            /* parse */
            current_case = parse(current_case, input_buffer, output_buffer + index);

            /* Show output */
            printf("%s", output_buffer);

            /* Prompt for a new command */
            printf("\nlocal > ");

        }

    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    clear_locks();
    case_manager();

}
