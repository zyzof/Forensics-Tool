/* Functions for the remote access client */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include "case.h"
#include "client.h"
#include "constants.h"
#include "cli.h"

pthread_mutex_t read_mutex;

void *threaded_text(void *p) {
    int the_socket = *(int *)p;
    char buffer[BUFFER_SIZE] = { '\0' };
    int n = 0;
    while(the_socket == *(int *)p) { /* While this open socket has not changed */
        pthread_mutex_lock(&read_mutex);
        n = read(the_socket, buffer, BUFFER_SIZE);
        pthread_mutex_unlock(&read_mutex);
        buffer[n] = '\0';
        if(n) {
            if(!strncmp(buffer, "__Exit__", 8)) { /* Server halted */
                *(int *)p = -1;
                break;
            }
            printf("%s", buffer);
            fflush(stdout);
        }
        buffer[0] = '\0';
    }
}

void remote_access(char *ip) {
    int connected = 0;
    char in_buffer[BUFFER_SIZE] = { '\0' };
    char out_buffer[BUFFER_SIZE] = { '\0' };
    struct addrinfo *target_address_information = NULL;
    int the_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    int n = 0;
    
    pthread_mutex_init(&read_mutex, NULL);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    pthread_t text_reading_thread;

    /* Parse IP address */
    if(getaddrinfo(ip, PORT_STRING, &hints, &target_address_information)) {
        fprintf(stderr, "Error: Could not resolve address.\n");
    }


    /* Request connection */
    if(connect(the_socket, target_address_information->ai_addr, target_address_information->ai_addrlen) != -1) {
        fprintf(stderr, "Error: Could not connect.\n");
    }
    else {
        connected = 1;
    }

    freeaddrinfo(target_address_information);

    printf("\nConnected to remote server.");

    pthread_create(&text_reading_thread, NULL, threaded_text, (void *)&the_socket);

    while(connected) {
        /* Read input */ /* This should be made non-blocking */
        printf("\nremote > ");
        gets(in_buffer);
        if(!strncmp(in_buffer, "exit", 5)) {
            /* Close the connection */
            connected = 0;
        }
        else if(!strncmp(in_buffer, "help", 5)) {
            main_menu_help();
        }
        else if(!strncmp(in_buffer, "connect ", 8)) {
            printf("Error: Disconnect from current server before trying to connect to another.\n");
        }
        else if(!strncmp(in_buffer, "server ", 7)) {
            printf("Error: Server must be controled locally.\n");
        }
        else if(!strncmp(in_buffer, "navigator", 10)) {
            printf("Sorry: File navigator only local.\n");
        }
        else { /* Send data to the remote program */
            write(the_socket, in_buffer, strnlen(in_buffer, BUFFER_SIZE));
        }

        /* A temporary hack... Give it time to reply. */
        usleep(100000);

        /* display output */
        pthread_mutex_lock(&read_mutex);
        n = read(the_socket, out_buffer, BUFFER_SIZE);
        pthread_mutex_unlock(&read_mutex);
        if (n < 0) {
            n = 0;
        }
        out_buffer[n] = '\0';

        if(!strncmp(out_buffer, "__Exit__", 8)) {
            the_socket = -1;
        }
        if(the_socket == -1) {
            connected = 0;
            break;
        }

        while(n > 0) {
            printf(out_buffer);
            pthread_mutex_lock(&read_mutex);
            n = read(the_socket, out_buffer, BUFFER_SIZE);
            pthread_mutex_unlock(&read_mutex);
            out_buffer[n] = '\0';
        }
        
    }

    printf("Connection closed.\n");
    
    pthread_mutex_destroy(&read_mutex);
}
