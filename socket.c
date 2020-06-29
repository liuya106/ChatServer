#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>     /* inet_ntoa */
#include <netdb.h>         /* gethostname */
#include <netinet/in.h>    /* struct sockaddr_in */

#include "socket.h"

void setup_server_socket(struct listen_sock *s) {
    if(!(s->addr = malloc(sizeof(struct sockaddr_in)))) {
        perror("malloc");
        exit(1);
    }
    // Allow sockets across machines.
    s->addr->sin_family = AF_INET;
    // The port the process will listen on.
    s->addr->sin_port = htons(SERVER_PORT);
    // Clear this field; sin_zero is used for padding for the struct.
    memset(&(s->addr->sin_zero), 0, 8);
    // Listen on all network interfaces.
    s->addr->sin_addr.s_addr = INADDR_ANY;

    s->sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s->sock_fd < 0) {
        perror("server socket");
        exit(1);
    }

    // Make sure we can reuse the port immediately after the
    // server terminates. Avoids the "address in use" error
    int on = 1;
    int status = setsockopt(s->sock_fd, SOL_SOCKET, SO_REUSEADDR,
        (const char *) &on, sizeof(on));
    if (status < 0) {
        perror("setsockopt");
        exit(1);
    }

    // Bind the selected port to the socket.
    if (bind(s->sock_fd, (struct sockaddr *)s->addr, sizeof(*(s->addr))) < 0) {
        perror("server: bind");
        close(s->sock_fd);
        exit(1);
    }

    // Announce willingness to accept connections on this socket.
    if (listen(s->sock_fd, MAX_BACKLOG) < 0) {
        perror("server: listen");
        close(s->sock_fd);
        exit(1);
    }
}

/* Insert Tutorial 10 helper functions here. */

int find_network_newline(const char *buf, int inbuf) {
    for (int i=0; i<inbuf; i++){
        if (buf[i]=='\r'){
            if (buf[i+1]=='\n'){
                return i+2;
            }
        }
    }
    return -1;
}

int read_from_socket(int sock_fd, char *buf, int *inbuf) {
    int read_byte = read(sock_fd, &(buf[*inbuf]), BUF_SIZE - *inbuf);
    (*inbuf) += read_byte;
    if ( (*inbuf) > BUF_SIZE||read_byte==-1){
        return -1;
    }
    
    if (read_byte==0){
        return 1;
    }

    if (find_network_newline(buf, *inbuf)!=-1){
        return 0;
    }
    return 2;
}

int get_message(char **dst, char *src, int *inbuf) {
    int new_line = find_network_newline(src, *inbuf);
    if (new_line == -1){
        return 1;
    }
    *dst = malloc(sizeof(char)*new_line);
    strncpy(*dst, src, new_line);
    (*dst)[new_line-2] = '\0';
    *inbuf -= new_line;
    memmove(src, &(src[new_line]), BUF_SIZE-new_line);
    // src = &(src[new_line]);
    return 0;
}

/* Helper function to be completed for Tutorial 11. */

int write_to_socket(int sock_fd, char *buf, int len) {
    int write_byte = write(sock_fd, buf, len);
    if (write_byte==len){
        return 0;
    }

    if (write_byte==0) {
        return 2;
    }
    
    return 1;
    
}
