#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "socket.h"
#include "chat_helpers.h"

int write_buf_to_client(struct client_sock *c, char *buf, int len) {
    // To be completed.    
    buf[len] = '\r';
    buf[len+1] = '\n';
    return write_to_socket(c->sock_fd, buf, len+2);
}

int remove_client(struct client_sock **curr, struct client_sock **clients) {
    if ((*clients)->sock_fd == (*curr)->sock_fd){
        *clients = (*clients)->next;
        *curr = *clients;
        return 0;
    }
    struct client_sock *loo = *clients;
    
    while (loo->next->sock_fd != (*curr)->sock_fd && loo != NULL){
        loo = loo->next;
    }
    if (loo->next==NULL){
        return 1; // Couldn't find the client in the list, or empty list
    }
    loo->next = (*curr)->next;
    *curr = loo->next;
    return 0;
}

int read_from_client(struct client_sock *curr) {
    return read_from_socket(curr->sock_fd, curr->buf, &(curr->inbuf));
}

int set_username(struct client_sock *curr) {
    //To be completed. Hint: Use get_message().
    char *name;
    if (get_message(&name, curr->buf, &(curr->inbuf))==0){
        curr->username = name;
        return 0;
    }else{
        return 1;
    }
}
