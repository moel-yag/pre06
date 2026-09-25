#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

typedef struct s_cli {
    int id;
    char msg[65535]; // message buffer
    int len;
}   t_client;

t_client clients[1024]; // array of clients

fd_set curr, rd, wr; // fd_set=>file descriptor set: current, read, write
int maxfd = 0;
int nextfd = 0;

void print(char *str){ write(2, str, strlen(str)); exit(1); } // print error message and exit

// send message to all clients except the sender
void sendall(int fd, char *msg){
    for (int i = 0; i <= maxfd; i++)
        if (FD_ISSET(i, &wr) && i != fd) // if the client is in write set and not the sender
            if (send(i, msg, strlen(msg), 0) < 0)
                print("Fatal error\n");
}

int main(int ac, char **av){
    if (ac != 2) print("Wrong number of arguments\n");
    int sockfd;
    struct sockaddr_in servaddr;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        print("Fatal error\n"); // create socket

    servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); // 127.0.0.1
	servaddr.sin_port = htons(atoi(av[1])); // port from argument
    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        print("Fatal error\n");
    if (listen(sockfd, 10) < 0)
        print("Fatal error\n"); // listen for connections
    
    FD_ZERO(&curr); // initialize current fd_set
    FD_SET(sockfd, &curr); // add server socket to current fd_set
    maxfd = sockfd; // set maxfd to server socket
    char recbuff[69555];
    char outbuff[70000];

    while (1){
        rd = wr = curr; // copy current fd_set to read and write fd_set
        if (select(maxfd + 1, &rd, &wr, NULL, NULL) < 0) continue; // wait for activity on sockets

        for (int fd = 0; fd <= maxfd; fd++){
            if (!FD_ISSET(fd, &rd)) continue; // if fd is not in read set, skip
            if (fd == sockfd){
                int cli = accept(fd, NULL, NULL);
                if (cli < 0) print("Fatal error");
                if (cli > maxfd) maxfd = cli;

                FD_SET(cli, &curr);  // add new client to current fd_set
                clients[cli].id = nextfd++;
                clients[cli].len = 0;

                sprintf(outbuff, "server: client %d just arrived\n", clients[cli].id);
                sendall(cli, outbuff);
            } else {
                int n = recv(fd, recbuff, sizeof(recbuff), 0);

                if (n <= 0){
                    sprintf(outbuff, "server: client %d just left\n", clients[fd].id);
                    sendall(fd, outbuff);
                    FD_CLR(fd, &curr); // remove client from current fd_set
                    close(fd);
                    continue;
                }
                for (int i = 0; i < n; i++){
                    clients[fd].msg[clients[fd].len++] = recbuff[i]; // append received character to client's message buffer
                    if (recbuff[i] == '\n'){ // if newline is received, send the message to all clients
                        clients[fd].msg[clients[fd].len - 1] = '\0';
                        sprintf(outbuff,
                            "client %d: %s\n",
                            clients[fd].id,
                            clients[fd].msg
                        );
                        sendall(fd, outbuff); // send message to all clients except the sender
                        clients[fd].len = 0;
                    }
                }
            }
        }
    }
}
