#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>

// global stuff
typedef struct	s_client
{
	int fd;
	char *buf;
}				t_client;

t_client clients[65535];
fd_set read_set;
fd_set write_set;
fd_set curr_read;
fd_set curr_write;
int max_fd = 0;
int next_id = 0;
int client_ids[65536];

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}


void fatal()
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

void send_to_all(int ex_fd, char *msg)
{
	for (int fd = 0; fd <= max_fd; fd++)
	{
		if (clients[fd].fd > 0 && fd != ex_fd)
			send(fd, msg, strlen(msg), 0);
	}
}

void accept_client(int server_fd)
{
	struct sockaddr_in new;
	socklen_t len = sizeof(new);
	int fd = accept(server_fd, (struct sockaddr *)&new, &len);
	if (fd < 0)
		return ;

	if (fd > max_fd)
		max_fd = fd;

	clients[fd].fd = fd;
	clients[fd].buf = NULL;
	client_ids[fd] = next_id++;

	FD_SET(fd, &read_set);
	FD_SET(fd, &write_set);

	char msg[64];
	sprintf(msg, "server: client %d just arrived\n", client_ids[fd]);
	send_to_all(fd, msg);
}

void remove_client(int fd)
{
	char msg[64];
	sprintf(msg, "server: client %d just left\n", client_ids[fd]);
	send_to_all(fd, msg);

	FD_CLR(fd, &read_set);
	FD_CLR(fd, &write_set);
	free(clients[fd].buf);
	clients[fd].buf = NULL;
	clients[fd].fd = 0;
	close(fd);
}

void handle_client(int fd)
{
	char tmp[4096];
	int ret = recv(fd, tmp, 4096, 0);
	if (ret <= 0)
	{
		remove_client(fd);
		return;
	}

	tmp[ret] = '\0';
	clients[fd].buf = str_join(clients[fd].buf, tmp);
	if (!clients[fd].buf)
		fatal();

	char *msg = NULL;
	while (extract_message(&clients[fd].buf, &msg) > 0)
	{
		char prefix[32];
		sprintf(prefix, "client %d: ", client_ids[fd]);

		char *out = malloc(strlen(prefix) + strlen(msg) + 1);
		if (!out)
			fatal();
		strcpy(out, prefix);
		strcat(out, msg);

		send_to_all(fd, out);
		free(out);
		free(msg);
	}
}

int main(int ac, char **av)
{
	if (ac != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}

	int server_fd;
	struct sockaddr_in addr;

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(0x7F000001);
	addr.sin_port = htons(atoi(av[1]));

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0)
		fatal();

	if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		fatal();

	if (listen(server_fd, 100) < 0)
		fatal();

	FD_ZERO(&read_set);
	FD_ZERO(&write_set);
	FD_SET(server_fd, &read_set);
	max_fd = server_fd;

	bzero(clients, sizeof(clients));
	bzero(client_ids, sizeof(client_ids));

	while (1)
	{
		curr_read = read_set;
		curr_write = write_set;

		select(max_fd + 1, &curr_read, &curr_write, NULL, NULL);

		if (FD_ISSET(server_fd, &curr_read))
			accept_client(server_fd);

		for (int fd = 0; fd <= max_fd; fd++)
		{
			if (fd != server_fd && clients[fd].fd > 0 && FD_ISSET(fd, &curr_read))
				handle_client(fd);
		}
	}
}