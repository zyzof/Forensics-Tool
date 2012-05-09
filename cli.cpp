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
#include "hexedit.h"
#include "filesearch.h"
#include "deletedFiles.h"
#include "sniff.h"
#include "navigator.h"

#define ESC 0x1B
#define RED 31
#define GREEN 32
#define WHITE 37
#define BRIGHT 1

using namespace std;

void title() {
    printf("%c[;H%c[J", ESC, ESC);
    printf("%c[%d;%dm", ESC, BRIGHT, GREEN); /* Set bright green text */
    printf("+-----------------------+\n");
    printf("|    Case Management    |\n");
    printf("+-----------------------+\n\n\n");
    printf("%c[0m", ESC); /* Restore default text */
}

void main_menu_help() {
    printf("\n");
    printf("Management commands\n");
    printf("-------------------\n");
    printf("help - Print this message.\n");
    printf("new <name> - Create a new case.\n");
    printf("list - List available cases.\n");
    printf("open <case> - Open a case to work on.\n");
    printf("close - Close the current case\n");
    printf("exit - Leave the progam.\n");
    printf("connect <ip_address> - Connect to a remote server.\n");
    printf("server <on / off> - Enable or disable remote access.\n");

    printf("\nForensic commands\n");
    printf("-----------------\n");
    printf("geometry <path> - Show the disk geometry for the device at <path>.\n");
    printf("partitions <path / filename> - Show the partition table for <path / filename>.\n");
    printf("deletedfiles - List the deleted files on a FAT32 or NTFS disk\n");

    printf("\nFile commands\n");
    printf("-----------------\n");
    printf("navigator - Open the file navigator.\n");
    printf("search - Search for files by directory and keyword.\n");
    printf("hexfind - Search for files by directory and hex string.\n");
    printf("hexedit - Edit a file with the hex editor.\n");

    printf("\nNetworking commands\n");
    printf("-----------------\n");
    printf("netdev - List available network devices.\n");
    printf("sniffer <on / off> - Enable or disable the packet sniffer.\n");
    printf("sniffer <show / hide> - Show or hide recent packets sniffed.\n");
    printf("psd <on / off> - Enable or disable port scan detection (sniffer must be on).\n");

    printf("\nCase commands\n");
    printf("-------------\n");
    printf("files - List files available for a case.\n");
    printf("note - Add a note to a case log.\n");
    printf("log - Show the work log for the current case.\n");
    printf("copy <file> <name> - Copy <file> from the filesysem to the case directory as <name>\n");
}

extern int server_running; 
extern pthread_t the_listener;

Case parse(Case current_case, char *input, char *output) {
    int disk_fd = 0;
    int index = 0;
    char buffer_B[BUFFER_SIZE] = { '\0' };

    /* connect */ /* Should be local only */
    if(!strncmp(input, "connect ", 8)) {
        if(current_case.log) {
            index += sprintf(output + index, "Error: Local cases should be closed before accessing a remote case.\n");
        }
        else {
            remote_access(input + 8);
            }
        }

    /* server */ /* Shold be controlled localy */
    else if(!strncmp(input, "server ", 7)) {
        if(!strncmp(input + 7, "on", 3)) {
            /* Turn on the server */
            if(!server_running) {
                server_running = 1;
                pthread_create(&the_listener, NULL, listener, NULL);
                index += sprintf(output + index, "Server started. (Port 1234)\n");
            }
            else {
                index += sprintf(output + index, "Error: Server is already on.\n");
            }
        }
        else if(!strncmp(input + 7, "off", 4)) {
            /* Turn off the server */
            index += sprintf(output + index, "Shutting down server... ");
            server_running = 0;
            /* === pthread joinr should go here === */
            index += sprintf(output + index, "Done.\n");
        }
        else {
            index += sprintf(output + index, "Error: Invalid option. Use \"on\" or \"off\".\n");
        }
    }

    /* new */ 
    else if(!strncmp(input, "new ", 4)) {
        if(!current_case.log) {
            current_case = new_case(input + 4, output + index, current_case);
        }
        else {
            index += sprintf(output + index, "Error: Only one case may be open at a time.\n");
        }
    }
            
    /* list */
    else if(!strncmp(input, "list", 5)) {
        index += list_cases(output + index);
    }
            
    /* files */
    else if(!strncmp(input, "files", 6)) {
        if(current_case.log) {
            index += list_files(current_case, output + index);
            log_text(current_case, "Files listed.");
        }
        else {
            index += sprintf(output + index, "Error: No case is currently open.\n");
        }
    }

    /* open <case name> */ 
    else if(!strncmp(input, "open ", 5)) {
        if(!current_case.log) {
            current_case = open_case(input + 5, output + index, current_case);
        }
        else {
            index += sprintf(output + index, "A case is already open. The current case must be closed before a new one can be accessed.\n");
        }
    }

    /* close */
    else if(!strncmp(input, "close", 6)) {
        current_case = close_case(current_case, output + index);
    }
            
    /* geometry */
    else if(!strncmp(input, "geometry ", 9)) {
        if(current_case.log) {
            disk_fd = open(input + 9, O_RDONLY | O_NONBLOCK);
            if(disk_fd == -1) {
                index += sprintf(output + index, "Error: Could not open %s.\n", input + 9);
            }
            index += print_geometry(disk_fd, PRINT_CHS, output + index);
            close(disk_fd);
            sprintf(buffer_B, "Accessed disk geometry of %s.\n", input + 9);
            log_text(current_case, buffer_B);
        }
        else { 
            index += sprintf(output + index, "Error: No case is currently open.\n");
        }
    }

    /* partitions */
    else if(!strncmp(input, "partitions ", 11)) {
        if(current_case.log) {
            sprintf(buffer_B, "%s", input + 11);
            file_string(current_case, buffer_B);
                    
            disk_fd = open(buffer_B, O_RDONLY | O_NONBLOCK);
            if(disk_fd == -1) {
                index += sprintf(output + index, "Error: Could not open %s.\n", input + 11);
            }
            index += print_primary(disk_fd, 0, output + index);
            close(disk_fd);
            index += sprintf(buffer_B, "Accessed partition table of %s.\n", output + index);
            log_text(current_case, buffer_B);
        }
        else { 
            index += sprintf(output + index, "Error: No case is currently open.\n");
        }
    }
    
    else if(!strncmp(input, "deletedfiles", 12)) {
		listDeletedFiles(current_case);
	}

	    /* Search functions */

	    /* String search */
	    else if(!strncmp(input, "search", 7)){
		if(current_case.log) {
		search(current_case, false);
		}
		else {
		index += sprintf(output + index, "Error: No case is open.\n");
		}
	    }

	    /* Hex search */
	    else if(!strncmp(input, "hexfind", 8)){
		if(current_case.log) {
		search(current_case, true);
		}
		else {
		index += sprintf(output + index, "Error: No case is open.\n");
		}
	    }

	    /* Hex edit */
	    else if(!strncmp(input, "hexedit", 8)){
		if(current_case.log) {
	        put_output(current_case, "File path");
		string filename = get_input_string(current_case);
		hexEdit(current_case, filename);
		}
		else {
		index += sprintf(output + index, "Error: No case is open.\n");
		}
	    }

    /* note */ /* To update... */
    else if(!strncmp(input, "note ", 5)) {
        if(current_case.log) {
            sprintf(buffer_B, "Note: %s", input + 5);
            log_text(current_case, buffer_B);
        }
        else {
            index += sprintf(output + index, "\nError: No case open.");
        }
    }

    /* log */ /* To update... */
    else if(!strncmp(input, "log", 4)) {
        if(current_case.log) {
            index += show_log(current_case, output + index);
        }
        else {
            index += sprintf(output + index, "Error: No case open.\n");
        }
    }
            
    /* copy */ /* To update... */
    else if(!strncmp(input, "copy ", 5)) {
        if(current_case.log) {
            sprintf(buffer_B, "%s", strrchr(input, ' ') + 1);
            *strrchr(input, ' ') = '\0'; /* Seperate strings */            
            index += copy(current_case, input + 5, buffer_B, output + index);
            
        }
        else {
            index += sprintf(output + index, "Error: No case open.\n");
        }
    }

    /* help */ /* Should be run locally */
    else if(!strncmp(input, "help", 5)) {
        main_menu_help();
    }

/* packet sniffer */
    else if (!strncmp(input, "sniffer ", 8)) {
        if (current_case.log) {
            if (!strncmp(input + 8, "on", 3)) {
                start_sniff(&current_case);
            }
            else if (!strncmp(input + 8, "off", 4)) {
                stop_sniff();
            }
            else if (!strncmp(input + 8, "show", 5)) {
                show_sniff();
            }
            else if (!strncmp(input + 8, "hide", 5)) {
                hide_sniff();
            }
            else {
                index += sprintf(output + index, "Error: Invalid option. Use \"on\", \"off\", \"show\" or \"hide\".\n");
            }
        }
        else {
            index += sprintf(output + index, "\nError: No case open.");
        }
    }
    else if (!strncmp(input, "netdev", 7)) {
        network_devices(&current_case);
    }
    else if (!strncmp(input, "psd ", 4)) {
        if (current_case.log) {
            if (!strncmp(input + 4, "on", 3)) {
                start_psd(&current_case);
            }
            else if (!strncmp(input + 4, "off", 4)) {
                stop_psd();
            }
            else {
                index += sprintf(output + index, "Error: Invalid option. Use \"on\" or \"off\".\n");
            }
        }
        else {
            index += sprintf(output + index, "\nError: No case open.");
        }
    }
    
    /* file navigator */
    else if (!strncmp(input, "navigator", 10)) {
        log_text(current_case, "File navigator opened.");
        run_file_navigator();
    }
    
    else {
        index += sprintf(output + index, "\nError: Unknown command entered.");
    }

    return current_case;
}

