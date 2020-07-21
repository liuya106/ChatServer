#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>


#include "socket.h"
#include "chat_helpers.h"

int sigint_received = 0;

void sigint_handler(int code) {
    sigint_received = 1;
}

/*
 * Wait for and accept a new connection.
 * Return -1 if the accept call failed.
 */
int accept_connection(int fd, struct client_sock **clients) {
    struct sockaddr_in peer;
    unsigned int peer_len = sizeof(peer);
    peer.sin_family = AF_INET;

    int num_clients = 0;
    struct client_sock *curr = *clients;
    while (curr != NULL && num_clients < MAX_CONNECTIONS && curr->next != NULL) {
        curr = curr->next;
        num_clients++;
    }

    int client_fd = accept(fd, (struct sockaddr *)&peer, &peer_len);
    
    if (client_fd < 0) {
        perror("server: accept");
        close(fd);
        exit(1);
    }

    if (num_clients == MAX_CONNECTIONS) {
        close(client_fd);
        return -1;
    }

    struct client_sock *newclient = malloc(sizeof(struct client_sock));
    newclient->sock_fd = client_fd;
    newclient->inbuf = newclient->state = 0;
    newclient->username = NULL;
    newclient->next = NULL;
    memset(newclient->buf, 0, BUF_SIZE);
    if (*clients == NULL) {
        *clients = newclient;
        (*clients)->state = 1;  //this client is the admin
    }
    else {
        curr->next = newclient;
    }

    return client_fd;
}

/*
 * Close all sockets, free memory, and exit with specified exit status.
 */
void clean_exit(struct listen_sock s, struct client_sock *clients, int exit_status) {
    struct client_sock *tmp;
    while (clients) {
        tmp = clients;
        close(tmp->sock_fd);
        clients = clients->next;
        free(tmp->username-1);
        free(tmp);
    }
    close(s.sock_fd);
    free(s.addr);
    exit(exit_status);
}

int main(void) {
    // This line causes stdout not to be buffered.
    // Don't change this! Necessary for autotesting.
    setbuf(stdout, NULL);
    
    /*
     * Turn off SIGPIPE: write() to a socket that is closed on the other
     * end will return -1 with errno set to EPIPE, instead of generating
     * a SIGPIPE signal that terminates the process.
     */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal");
        exit(1);
    }

    // Linked list of clients
    struct client_sock *clients = NULL;
    
    struct listen_sock s;
    setup_server_socket(&s);
    
    // Set up SIGINT handler
    struct sigaction sa_sigint;
    memset (&sa_sigint, 0, sizeof (sa_sigint));
    sa_sigint.sa_handler = sigint_handler;
    sa_sigint.sa_flags = 0;
    sigemptyset(&sa_sigint.sa_mask);
    sigaction(SIGINT, &sa_sigint, NULL);
    
    int exit_status = 0;
    
    int max_fd = s.sock_fd;

    fd_set all_fds, listen_fds;
    
    FD_ZERO(&all_fds);
    FD_SET(s.sock_fd, &all_fds);

    do {
        listen_fds = all_fds;
        int nready = select(max_fd + 1, &listen_fds, NULL, NULL, NULL);
        if (sigint_received) break;
        if (nready == -1) {
            if (errno == EINTR) continue;
            perror("server: select");
            exit_status = 1;
            break;
        }

        /* 
         * If a new client is connecting, create new
         * client_sock struct and add to clients linked list.
         */
        if (FD_ISSET(s.sock_fd, &listen_fds)) {
            int client_fd = accept_connection(s.sock_fd, &clients);
            if (client_fd < 0) {
                printf("Failed to accept incoming connection.\n");
                continue;
            }
            if (client_fd > max_fd) {
                max_fd = client_fd;
            }
            FD_SET(client_fd, &all_fds);
            printf("Accepted connection\n");
        }

        if (sigint_received) break;

        /*
         * Accept incoming messages from clients,
         * and send to all other connected clients.
         */
        struct client_sock *curr = clients;
        while (curr) {
            if (!FD_ISSET(curr->sock_fd, &listen_fds)) {
                curr = curr->next;
                continue;
            }
            int client_closed = read_from_client(curr);
            
            // If error encountered when receiving data
            if (client_closed == -1) {
                perror("read");
                client_closed = 1; // Disconnect the client
            }
            
            // If received at least one complete message
            // and client is newly connected: Get username
            if (client_closed == 0 && curr->username == NULL) {
                if (set_username(curr)) {
                    printf("Error processing user name from client %d.\n", curr->sock_fd);
                    client_closed = 1; // Disconnect the client
                }
                else {
                    struct client_sock *name = clients;
                    while(name->next!=NULL && (name->sock_fd == curr->sock_fd || strcmp(name->username, curr->username))){
                        name = name->next;
                    }
                    if (!strcmp(curr->username, "SERVER")||(!strcmp(name->username, curr->username) && name->sock_fd != curr->sock_fd)){
                        char errorname[45] = "1SERVER Username invalid or already taken.\n";
                        write_buf_to_client(curr, errorname, 43);
                        printf("Error processing user name from client %d.\n", curr->sock_fd);
                        client_closed = 1; // Disconnect the client
                    }else{
                        printf("Client %d user name is %s.\n", curr->sock_fd, curr->username);
                    }
                }
            }

            
            char *msg;
            char write_buf[BUF_SIZE];
            int data_len;
            char notification[100];
            int noti_len;
            // Loop through buffer to get complete message(s)
            while (client_closed == 0 && !get_message(&msg, curr->buf, &(curr->inbuf))) {
                if (msg[0]=='0'&& curr->state==1){
                    char *kick = &(msg[1]);
                    struct client_sock *find = clients;
                    while(strcmp(find->username, kick) && find->next!=NULL){
                        find = find->next;
                    } 
                    //printf("%s %ld, %s %ld", find->username, strlen(find->username), kick, strlen(kick));
                    if (!strcmp(find->username, kick)){
                        if (find->sock_fd == curr->sock_fd){
                            client_closed = 1;
                            break;
                        }else{
                            FD_CLR(find->sock_fd, &all_fds);
                            close(find->sock_fd);
                            printf("Client %d disconnected\n", find->sock_fd);
                            remove_client(&find, &clients);
                        }
                    }
                    continue;
                }else if (msg[0]=='1'){
                    write_buf[0] = '1';
                }else if (msg[0] == '2'){   //image was sent
                    notification[0] = '\0';
                    strncat(notification, "1", MAX_NAME);
                    strncat(notification, curr->username, MAX_NAME);
                    strncat(notification, " sent an emote:\n", 20);
                    noti_len = strlen(notification);

                    write_buf[0] = '2';
                }
                write_buf[1] = '\0';
                printf("Echoing message from %s.\n", curr->username);
                strncat(write_buf, curr->username, MAX_NAME);
                strncat(write_buf, " ", MAX_NAME);
                strncat(write_buf, &(msg[1]), MAX_IMG_LEN);
                
                data_len = strlen(write_buf);

                struct client_sock *dest_c = clients;
                while (dest_c) {
                    if (dest_c != curr) {
                        if (write_buf[0]=='2'){
                            write_buf_to_client(dest_c, notification, noti_len);
                        }
                        //printf("%s\n", write_buf);    //newly added
                        int ret = write_buf_to_client(dest_c, write_buf, data_len);
                        
                        if (ret == 0) {
                            printf("Sent message from %s (%d) to %s (%d).\n",
                                curr->username, curr->sock_fd,
                                dest_c->username, dest_c->sock_fd);
                        }
                        else {
                            printf("Failed to send message to user %s (%d).\n", dest_c->username, dest_c->sock_fd);
                            if (ret == 2) {
                                printf("User %s (%d) disconnected.\n", dest_c->username, dest_c->sock_fd);
                                assert(remove_client(&dest_c, &clients) == 0); // If this fails we have a bug
                                continue;
                            }
                        }
                    }
                    dest_c = dest_c->next;
                }
                free(msg);
            }
            
            if (client_closed == 1) { // Client disconnected
                // Note: Never reduces max_fd when client disconnects
                FD_CLR(curr->sock_fd, &all_fds);
                close(curr->sock_fd);
                printf("Client %d disconnected\n", curr->sock_fd);
                assert(remove_client(&curr, &clients) == 0); // If this fails we have a bug
            }
            else {
                curr = curr->next;
            }
        }
    } while(!sigint_received);
    
    clean_exit(s, clients, exit_status);
}
