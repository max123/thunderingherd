
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <strings.h>

int
main(int argc, char *argv[])
{
	int sockfd;
	int rval;
	struct sockaddr_in mysockaddr;
	struct hostent *hp;
	int one = 1;

	bzero(&mysockaddr,sizeof(mysockaddr));

	if (argc == 2)
		hp = (struct hostent *) gethostbyname(argv[1]);
	else
		hp = (struct hostent *) gethostbyname("localhost");

	if (hp == (struct hostent *)NULL) {
		fprintf(stderr, "failed to get IP address for host: %s\n", argv[1]);
		exit(1);
	}
	mysockaddr.sin_family = hp->h_addrtype;
	memcpy(&mysockaddr.sin_addr, hp->h_addr, hp->h_length);
	mysockaddr.sin_port = htons(4321);

	for (;;) {
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0) {
			fprintf(stderr, "socket fail\n");
			exit(1);
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(one)) < 0) {
			fprintf(stderr, "setsockopt failed, errno = %d\n", errno);
			exit(1);
		}

		if (connect(sockfd, (struct sockaddr * ) &mysockaddr, sizeof(mysockaddr)) == 0) {
			close(sockfd);
		}
	}
}
