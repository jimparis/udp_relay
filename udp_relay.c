#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <err.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int get_address(const char *hostport, struct sockaddr **addr_out,
                socklen_t *addrlen_out)
{
        char *host;
        char *port;
        int ret;

        host = strdup(hostport);
        if (host == NULL)
                err(1, "strdup");
        port = strrchr(host, ':');
        if (port != NULL) {
                *port = '\0';
                port++;
        }
        if (port == NULL || strlen(host) == 0 || strlen(port) == 0)
                errx(1, "badly formated address \"%s\"; use ip:port", hostport);

        struct addrinfo hints = {
                .ai_family = AF_UNSPEC,
                .ai_socktype = SOCK_DGRAM,
                .ai_protocol = IPPROTO_UDP,
        };

        struct addrinfo *result, *rp;
        if ((ret = getaddrinfo(host, port, &hints, &result)) != 0)
                errx(1, "%s: %s", hostport, gai_strerror(ret));
        if (result == NULL)
                errx(1, "can't resolve %s", hostport);
        int sfd;
        for (rp = result; rp != NULL; rp = rp->ai_next)
        {
                sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
                if (sfd == -1)
                        continue;

                /* success -- copy sockaddr */
                *addr_out = malloc(rp->ai_addrlen);
                if (addr_out == NULL)
                        err(1, "malloc");
                memcpy(*addr_out, rp->ai_addr, rp->ai_addrlen);
                *addrlen_out = rp->ai_addrlen;
                break;
        }
        if (rp == NULL)
                errx(1, "can't bind listen server");

        freeaddrinfo(result);
        free(host);

        return sfd;
}

int main(int argc, char **argv)
{
	if (argc < 3)
		errx(1, "usage: %s <src:port> <dest:port> [dest:port...]");

        struct sockaddr *server_addr;
        socklen_t server_addrlen;
        int server_fd;
        server_fd = get_address(argv[1], &server_addr, &server_addrlen);
        if (bind(server_fd, server_addr, server_addrlen) != 0)
                err(1, "bind");

        int i;
        int n_cli = argc - 2;

        struct sockaddr *client_addr[n_cli];
        socklen_t client_addrlen[n_cli];
        int client_fd[n_cli];
        for (i = 0; i < n_cli; i++)
                client_fd[i] = get_address(argv[2 + i], &client_addr[i],
                                           &client_addrlen[i]);

	char buf[65536];
        bool first = true;
        for (;;)
        {
                int n = recv(server_fd, buf, sizeof(buf), 0);
                if (n < 0)
                        continue;
                if (first) {
                        printf("got first packet (%d bytes)\n", n);
                        first = false;
                }
                for (i = 0; i < n_cli; i++) {
                        int ret = sendto(client_fd[i], buf, n, 0,
                                         client_addr[i], client_addrlen[i]);
                        if (ret < 0) {
                                warn("sendto (%d)", i);
                        } else if (ret != n) {
                                warnx("short send (%d): %d/%d", i, ret, n);
                        }
                }
        }
}

