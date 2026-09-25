#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

typedef struct s_cli {
    int id;
    char msg[65535];
    int len;
} t_client;

t_client clients[1024];
fd_set curr, rd, wr;
int maxfd = 0;
int nextid = 0;

void fatal(void) { write(2, "Fatal error\n", 12); exit(1); }

void sendall(int fd, char *msg)
{
    for (int i = 0; i <= maxfd; i++)
        if (i != fd && FD_ISSET(i, &wr))
            if (send(i, msg, strlen(msg), 0) < 0)
                fatal();
}

int main(int ac, char **av)
{
    if (ac != 2) { write(2, "Wrong number of arguments\n", 26); exit(1); }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) fatal();

    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = htonl(2130706433); /* 127.0.0.1 */
    serv.sin_port = htons(atoi(av[1]));
    if (bind(sockfd, (struct sockaddr *)&serv, sizeof(serv)) < 0) fatal();
    if (listen(sockfd, 128) < 0) fatal();

    FD_ZERO(&curr);
    FD_SET(sockfd, &curr);
    maxfd = sockfd;

    char recbuf[65535];
    char outbuf[70000];

    while (1)
    {
        rd = wr = curr;
        if (select(maxfd + 1, &rd, &wr, NULL, NULL) < 0) continue;

        for (int fd = 0; fd <= maxfd; fd++)
        {
            if (!FD_ISSET(fd, &rd)) continue;

            if (fd == sockfd)
            {
                int cli = accept(sockfd, NULL, NULL);
                if (cli < 0) continue;
                FD_SET(cli, &curr);
                if (cli > maxfd) maxfd = cli;
                clients[cli].id = nextid++;
                clients[cli].len = 0;
                sprintf(outbuf, "server: client %d just arrived\n", clients[cli].id);
                sendall(cli, outbuf);
            }
            else
            {
                int n = recv(fd, recbuf, sizeof(recbuf), 0);
                if (n <= 0)
                {
                    sprintf(outbuf, "server: client %d just left\n", clients[fd].id);
                    sendall(fd, outbuf);
                    FD_CLR(fd, &curr);
                    close(fd);
                    continue;
                }
                for (int i = 0; i < n; i++)
                {
                    clients[fd].msg[clients[fd].len++] = recbuf[i];
                    if (recbuf[i] == '\n')
                    {
                        clients[fd].msg[clients[fd].len - 1] = '\0';
                        sprintf(outbuf, "client %d: %s\n", clients[fd].id, clients[fd].msg);
                        sendall(fd, outbuf);
                        clients[fd].len = 0;
                    }
                }
            }
        }
    }
    return 0;
}
