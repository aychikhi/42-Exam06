#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>

int count = 0, max_fd = 0, ids[65536];
char *msgs[65536];
fd_set rfds, wfds, afds;
char buf_read[1001], buf_write[42];

void fatal() 
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

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

void notify(int author, char *str) 
{
	for (int fd = 0; fd <= max_fd; fd++)
		if (FD_ISSET(fd, &wfds) && fd != author)
			send(fd, str, strlen(str), 0);
}

int main(int ac, char **av) {
	if (ac != 2) 
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) fatal();

	struct sockaddr_in serv;
	bzero(&serv, sizeof(serv));
	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = htonl(2130706433);
	serv.sin_port = htons(atoi(av[1]));

	if (bind(sockfd, (struct sockaddr *)&serv, sizeof(serv)) || listen(sockfd, 128)) fatal();
	
	FD_ZERO(&afds);
	FD_SET(sockfd, &afds);
	max_fd = sockfd;

	while (1) 
	{
		rfds = wfds = afds;
		if (select(max_fd + 1, &rfds, &wfds, NULL, NULL) < 0) fatal();
		
		for (int fd = 0; fd <= max_fd; fd++) 
		{
			if (!FD_ISSET(fd, &rfds)) continue;
			
			if (fd == sockfd) 
			{
				int cli = accept(sockfd, NULL, NULL);
				if (cli >= 0) 
				{
					max_fd = cli > max_fd ? cli : max_fd;
					ids[cli] = count++;
					msgs[cli] = NULL;
					FD_SET(cli, &afds);
					sprintf(buf_write, "server: client %d just arrived\n", ids[cli]);
					notify(cli, buf_write);
					break;
				}
			}
			else 
			{
				int read_bytes = recv(fd, buf_read, 1000, 0);
				if (read_bytes <= 0) 
				{
					sprintf(buf_write, "server: client %d just left\n", ids[fd]);
					notify(fd, buf_write);
					free(msgs[fd]);
					FD_CLR(fd, &afds);
					close(fd);
					break;
				}
				buf_read[read_bytes] = '\0';
				msgs[fd] = str_join(msgs[fd], buf_read);
				
				char *msg;
				while (extract_message(&msgs[fd], &msg)) 
				{
					sprintf(buf_write, "client %d: ", ids[fd]);
					notify(fd, buf_write);
					notify(fd, msg);
					free(msg);
				}
			}
		}
	}
	return 0;
}