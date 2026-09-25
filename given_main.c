#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

typedef struct s_client
{
	int id;
	char *in;
	char *out;
} t_client;

t_client clients[1024];
fd_set current, readfds, writefds;
int maxfd, nextid, serverfd;

void fatal(char *message)
{
	write(2, message, strlen(message));
	exit(1);
}

char *join(char *old, char *add)
{
	char *new;
	int len;

	len = old ? strlen(old) : 0;
	new = malloc(len + strlen(add) + 1);
	if (!new)
		fatal("Fatal error\n");
	new[0] = 0;
	if (old)
		strcat(new, old);
	strcat(new, add);
	free(old);
	return (new);
}

void send_all(int from, char *message)
{
	int fd;

	fd = 0;
	while (fd <= maxfd)
	{
		if (fd != from && fd != serverfd && FD_ISSET(fd, &current))
			clients[fd].out = join(clients[fd].out, message);
		fd++;
	}
}

void flush(int fd)
{
	char *rest;
	int sent;
	int len;

	if (!clients[fd].out || !FD_ISSET(fd, &writefds))
		return ;
	len = strlen(clients[fd].out);
	sent = send(fd, clients[fd].out, len, 0);
	if (sent < 0)
		fatal("Fatal error\n");
	if (sent == len)
	{
		free(clients[fd].out);
		clients[fd].out = 0;
	}
	else if (sent > 0)
	{
		rest = malloc(len - sent + 1);
		if (!rest)
			fatal("Fatal error\n");
		strcpy(rest, clients[fd].out + sent);
		free(clients[fd].out);
		clients[fd].out = rest;
	}
}

int main(int argc, char **argv)
{
	int server;
	int fd;
	int client;
	int n;
	int port;
	char input[65536];
	char message[70000];
	char *line;
	struct sockaddr_in address;

	if (argc != 2)
		fatal("Wrong number of arguments\n");
	server = socket(AF_INET, SOCK_STREAM, 0);
	if (server < 0)
		fatal("Fatal error\n");
	port = atoi(argv[1]);
	bzero(&address, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = 0x0100007f;
	address.sin_port = (port >> 8) | (port << 8);
	if (bind(server, (struct sockaddr *)&address, sizeof(address)) < 0
		|| listen(server, 10) < 0)
		fatal("Fatal error\n");
	serverfd = server;
	FD_ZERO(&current);
	FD_SET(server, &current);
	maxfd = server;
	nextid = 0;
	while (1)
	{
		readfds = current;
		writefds = current;
		fd = 0;
		while (fd <= maxfd)
		{
			if (!clients[fd].out)
				FD_CLR(fd, &writefds);
			fd++;
		}
		if (select(maxfd + 1, &readfds, &writefds, 0, 0) < 0)
			fatal("Fatal error\n");
		if (FD_ISSET(server, &readfds))
		{
			client = accept(server, 0, 0);
			if (client < 0)
				fatal("Fatal error\n");
			clients[client].id = nextid++;
			clients[client].in = 0;
			clients[client].out = 0;
			FD_SET(client, &current);
			if (client > maxfd)
				maxfd = client;
			sprintf(message, "server: client %d just arrived\n", clients[client].id);
			send_all(client, message);
		}
		fd = 0;
		while (fd <= maxfd)
		{
			if (fd != server && FD_ISSET(fd, &readfds))
			{
				n = recv(fd, input, sizeof(input) - 1, 0);
				if (n <= 0)
				{
					sprintf(message, "server: client %d just left\n", clients[fd].id);
					send_all(fd, message);
					free(clients[fd].in);
					free(clients[fd].out);
					FD_CLR(fd, &current);
					close(fd);
				}
				else
				{
					input[n] = 0;
					clients[fd].in = join(clients[fd].in, input);
					while (clients[fd].in && strstr(clients[fd].in, "\n"))
					{
						line = clients[fd].in;
						clients[fd].in = strstr(line, "\n") + 1;
						clients[fd].in[-1] = 0;
						sprintf(message, "client %d: %s\n", clients[fd].id, line);
						send_all(fd, message);
						clients[fd].in = join(0, clients[fd].in);
						free(line);
					}
				}
			}
			fd++;
		}
		fd = 0;
		while (fd <= maxfd)
		{
			if (FD_ISSET(fd, &current))
				flush(fd);
			fd++;
		}
	}
}
