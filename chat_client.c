#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "socket.h"

struct server_sock {
    int sock_fd;
    char buf[BUF_SIZE];
    int inbuf;
};

int main(int argc, char **argv) {
    char *host_ip = "127.0.0.1";
    struct server_sock s;
    s.inbuf = 0;
    int exit_status = 0;
    if (argc==2) {
        host_ip = argv[1];
    }
    
    // Create the socket FD.
    s.sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s.sock_fd < 0) {
        perror("client: socket");
        exit(1);
    }

    // Set the IP and port of the server to connect to.
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, host_ip, &server.sin_addr) < 1) {
        perror("client: inet_pton");
        close(s.sock_fd);
        exit(1);
    }

    // Connect to the server.
    if (connect(s.sock_fd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("client: connect");
        close(s.sock_fd);
        exit(1);
    }

    char *buf = NULL; // Buffer to read name from stdin
    int name_valid = 0;
    while(!name_valid) {
        printf("Please enter a username: ");
        fflush(stdout);
        size_t buf_len = 0;
        size_t name_len = getline(&buf, &buf_len, stdin);
        if (name_len < 0) {
            perror("getline");
            fprintf(stderr, "Error reading username.\n");
            free(buf);
            exit(1);
        }
        
        if (name_len - 1 > MAX_NAME) { // name_len includes '\n'
            printf("Username can be at most %d characters.\n", MAX_NAME);
        }
        else {
            // Replace LF+NULL with CR+LF
            char protocol[MAX_NAME+3] = "1";
            strncat(protocol, buf, name_len-1);
            strncat(protocol, "\r", MAX_NAME);
            strncat(protocol, "\n", MAX_NAME);
            if (write_to_socket(s.sock_fd, protocol, name_len+2)) {
                fprintf(stderr, "Error sending username.\n");
                free(buf);
                exit(1);
            }
            name_valid = 1;
            free(buf);
        }
    }
    
    /*
     * See here for why getline() is used above instead of fgets():
     * https://wiki.sei.cmu.edu/confluence/pages/viewpage.action?pageId=87152445
     */
    
    /*
     * Step 1: Prepare to read from stdin as well as the socket,
     * by setting up a file descriptor set and allocating a buffer
     * to read into. It is suggested that you use buf for saving data
     * read from stdin, and s.buf for saving data read from the socket.
     * Why? Because note that the maximum size of a user-sent message
     * is MAX_USR_MSG + 2, whereas the maximum size of a server-sent
     * message is MAX_NAME + 1 + MAX_USER_MSG + 2. Refer to the macros
     * defined in socket.h.
     */
    

    

    int numfd;
    if (fileno(stdin)>s.sock_fd){
        numfd = fileno(stdin)+1;
    }else{
        numfd = s.sock_fd+1;
    }

    
    /*
     * Step 2: Using select, monitor the socket for incoming mesages
     * from the server and stdin for data typed in by the user.
     */
    while(1) {
        char buffer[BUF_SIZE];
        char img[BUF_SIZE];

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(fileno(stdin), &read_fds);
        FD_SET(s.sock_fd, &read_fds);
        
        if (select(numfd, &read_fds, NULL, NULL, NULL) == -1){
            perror("Select");
            exit(1);
        }

        
        /*
         * Step 3: Read user-entered message from the standard input
         * stream. We should read at most MAX_USR_MSG bytes at a time.
         * If the user types in a message longer than MAX_USR_MSG,
         * we should leave the rest of the message in the standard
         * input stream so that we can read it later when we loop
         * back around.
         * 
         * In other words, an oversized messages will be split up
         * into smaller messages. For example, assuming that
         * MAX_USR_MSG is 10 bytes, a message of 22 bytes would be
         * split up into 3 messages of length 10, 10, and 2,
         * respectively.
         * 
         * It will probably be easier to do this using a combination of
         * fgets() and ungetc(). You probably don't want to use
         * getline() as was used for reading the user name, because
         * that would read all the bytes off of the standard input
         * stream, even if it exceeds MAX_USR_MSG.
         */
        if (FD_ISSET(fileno(stdin), &read_fds)){
            if (fgets(buffer, MAX_USER_MSG+2, stdin)==NULL){
                perror("fgets");
                exit(1);
            }else{
                
                if (buffer[0]=='.'){
                    if (buffer[1]=='k' && buffer[2]== ' '){
                        buffer[2] = '0';
                        buffer[strlen(buffer)-1] = '\0';                 //remove the newline
                        memmove(buffer, &(buffer[2]), BUF_SIZE-2);

                    }else if(buffer[1]=='e' &&buffer[2] == ' '){             //sending image
                        buffer[strlen(buffer)-1] = '\0';                 //remove the newline
                        memmove(buffer, &(buffer[3]), BUF_SIZE-3);
                        
                        int fd[2];
                        if (pipe(fd)==-1){
                            perror("pipe");
                            exit(1);
                        }
                        int f = fork();

                        if (f>0){
                            close(fd[1]);
                            int red = read(fd[0], img, MAX_IMG_LEN);
                            if (red > MAX_IMG_LEN||red==-1){
                                perror("read");
                                exit(1);
                            }
                            char protocol[BUF_SIZE] = "2";
                            strncat(protocol, img, BUF_SIZE); 
                            strncat(protocol, "\r", BUF_SIZE);
                            strncat(protocol, "\n", BUF_SIZE);
                            int protocol_len = strlen(protocol);
                            //printf("%s", protocol);           //newly added
                            int wrote = write_to_socket(s.sock_fd, protocol, protocol_len);
                            if (wrote == 1){
                                perror("write_to_socket");
                                exit_status = 1;
                                break;
                            }else if(wrote == 2){
                                break;
                            }
                            close(fd[0]);
                            continue;
                        }else if(f==0){
                            close(fd[0]);
                            char path[MAX_USER_MSG+9] = "./emotes/";
                            strncat(path, buffer, MAX_USER_MSG-3);
                            dup2(fd[1], fileno(stdout));
                            if (execlp("base64", "base64", "-w0", (char*)path, (char *)NULL)==-1){
                                printf("Error: Emote image not found");
                            }
                            close(fd[1]);
                            exit(0);
                        }else{
                            perror("fork");
                            exit(1);
                        }

                    }else{
                        char protocol[BUF_SIZE] = "1";
                        strncat(protocol, buffer, strlen(buffer));
                        strcpy(buffer, protocol);
                    }

                }else{
                    char protocol[BUF_SIZE] = "1";
                    strncat(protocol, buffer, strlen(buffer));
                    strcpy(buffer, protocol);
                }
                
                int w;
                int len = strlen(buffer);
                if (len!=MAX_USER_MSG+2){
                    strncat(buffer, "\r", BUF_SIZE);
                    strncat(buffer, "\n", BUF_SIZE);
                    w= write_to_socket(s.sock_fd, buffer, len+2);
                }else{
                    if (buffer[MAX_USER_MSG+1]!='\n'){
                        ungetc(buffer[MAX_USER_MSG+1], stdin);
                        buffer[MAX_USER_MSG+1] = '\n';
                    }
                    buffer[MAX_USER_MSG+2] = '\r';
                    buffer[MAX_USER_MSG+3] = '\n';
                    w = write_to_socket(s.sock_fd, buffer, MAX_USER_MSG+4);
                }
                if (w == 1){
                    perror("write_to_socket");
                    exit_status = 1;
                    break;
                }else if(w == 2){
                    break;
                }
            }
        }

        /*
         * Step 4: Read server-sent messages from the socket.
         * The read_from_socket() and get_message() helper functions
         * will be useful here. This will look similar to the
         * server-side code.
         */

        if (FD_ISSET(s.sock_fd, &read_fds)){
            char *msg;
            int r = read_from_socket(s.sock_fd, s.buf, &(s.inbuf));
            if (r == -1){
                exit_status = 1;
                perror("read");
                break;
            }else if(r == 1){
                perror("disconnected");
                break;
            }else if(r==0){
                while(!get_message(&msg, s.buf, &(s.inbuf))){
                    if (msg[0]=='1'){
                        printf("%s", &(msg[1]));
                    }else if(msg[0]=='2'){
                        char *pic = strchr(msg, ' ');
                        //printf("%s", &(pic[1]));      //newly added
                        char pip_path[100]="./emotepipe.jpg";
                        
                        int fo = fork();
                        if (fo>0){
                            if (mkfifo(pip_path, 0666)==-1){
                                perror("mkfifo");
                                exit(1);
                            }
                            wait(NULL);
                            if (unlink(pip_path)==-1){
                                perror("pip_path");
                                exit(1);
                            }
                        }else if (fo==0){
                            fo = fork();
                            if (fo>0){
                                int filefd1 = open(pip_path, O_RDONLY);
                                if (filefd1==-1){
                                    perror("open");
                                    exit(1);
                                }
                                execlp("catimg", "catimg", "-w80", pip_path, (char *)NULL);
                                close(filefd1);
                                exit(0);
                            }else if(fo==0){
                                int pip[2];
                                if (pipe(pip)==-1){
                                    perror("pipe");
                                    exit(1);
                                }
                                fo = fork();
                                if (fo>0){
                                    wait(NULL);
                                    close(pip[1]);
                                    char image[BUF_SIZE];
                                    if (read(pip[0], image, sizeof(image))==-1){
                                        perror("read");
                                        exit(1);
                                    }
                                    //printf("!!%s!!", image);           //new
                                    int filefd2 = open(pip_path, O_WRONLY);
                                    if (filefd2==-1){
                                        perror("open");
                                        exit(1);
                                    }
                                    write(filefd2, image, sizeof(image));
                                    close(pip[0]);
                                    close(filefd2);
                                    exit(0);
                                }else if (fo==0){
                                    int pi[2];
                                    if (pipe(pi)==-1){
                                        perror("pipe");
                                        exit(1);
                                    }
                                    fo = fork();
                                    if (fo>0){
                                        wait(NULL);
                                        dup2(pi[0], fileno(stdin));
                                        dup2(pip[1], fileno(stdout));
                                        close(pip[0]);
                                        close(pi[1]);
                                        close(pip[1]);
                                        close(pi[0]);
                                        execlp("base64", "base64", "-d", (char *)NULL);
                                        exit(0);
                                    }else if(fo==0){
                                        dup2(pi[1], fileno(stdout));
                                        close(pip[0]);
                                        close(pip[1]);
                                        close(pi[0]);
                                        close(pi[1]);
                                        execlp("echo", "echo", &(pic[1]), (char *)NULL);
                                        
                                    }
                                    
                                }
                            }
                        }
                    }
                    free(msg);
                }
            }
        }
    }
    
    close(s.sock_fd);
    exit(exit_status);
}
