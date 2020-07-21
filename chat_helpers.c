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
        if ((*clients)->next) {           //change the next client state to 1 
            (*clients)->next->state = 1;  //when first was removed
        }
        free((*clients)->username-1);
        struct client_sock *temp = (*clients)->next;
        free(*clients);
        *clients = temp;
        *curr = *clients;
        return 0;
    }
    struct client_sock *loo = *clients;
    
    while (loo->next->sock_fd != (*curr)->sock_fd && loo->next != NULL){
        loo = loo->next;
    }
    if (loo->next==NULL){
        return 1; // Couldn't find the client in the list, or empty list
    }
    free(loo->next->username-1);
    struct client_sock *temp = (*curr)->next;
    free(loo->next);
    loo->next = temp;
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
        curr->username = &(name[1]);
        return 0;
    }else{
        return 1;
    }
}
