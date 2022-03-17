#include <arpa/inet.h>
#include <getopt.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define PORT 8080

struct interface {
	char* name;
	int size;
	char* address[10];
};

/**
 * Get the prefix from an ipv4 subnet mask address.
 */
unsigned int getPrefix4(struct in_addr* sa) {
	int prefix = 0;

	while (sa->s_addr > 0) {
		if (sa->s_addr & 0x1) prefix++;

		sa->s_addr = sa->s_addr >> 1;
	}

	return prefix;
}

/**
 * Get the prefix from an ipv6 subnet mask address.
 */
unsigned int getPrefix6(struct in6_addr* sa) {
	int prefix = 0;
	int i = 0;
	int b = 0;

	/* Parse through every bits of the address. */
	while (((sa->s6_addr[i] >> b) & 1) > 0 && i < 16) {
		prefix++;
		b = (b + 1) % 8;

		if (b == 0) i++;
	}

	return prefix;
}

char* getAddress(char* interface) {
	if (interface == NULL) interface = "";

	struct ifaddrs* ifaddr;
	int family;

	struct interface* interfaces[NI_MAXHOST];
	int size = 0;

	if (getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs");
		exit(EXIT_FAILURE);
	}

	/* Walk through linked list, maintaining head pointer so we
	   can free list later. */
	for (struct ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL)
			continue;

		if (strcmp(interface, "") != 0 && strcmp(interface, ifa->ifa_name) != 0)
			continue;

		family = ifa->ifa_addr->sa_family;

		/* For an AF_INET* interface address, insert the address
		   into the interface name list. */
		if (family == AF_INET || family == AF_INET6) {
			struct interface* interface = NULL;
			for (int i = 0; i < size; i++) {
				if (strcmp(interfaces[i]->name, ifa->ifa_name) == 0) {
					interface = interfaces[i];
					break;
				}
			}

			if (interface == NULL) {
				interface = malloc(sizeof(struct interface));
				interface->name = ifa->ifa_name;
				interface->size = 0;
				interfaces[size++] = interface;
			}

			if (family == AF_INET) {
				struct sockaddr_in* ipv4 = (struct sockaddr_in*)ifa->ifa_addr;
				struct sockaddr_in* maskv4 = (struct sockaddr_in*)ifa->ifa_netmask;
				char* str_ipv4;

				str_ipv4 = inet_ntoa(ipv4->sin_addr);

				char str[32];
				sprintf(str, "%s/%u", str_ipv4, getPrefix4(&maskv4->sin_addr));
				interface->address[interface->size] = malloc(sizeof(str));
				strcpy(interface->address[interface->size++], str);
			} else if (family == AF_INET6) {
				struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)ifa->ifa_addr;
				struct sockaddr_in6* maskv6 = (struct sockaddr_in6*)ifa->ifa_netmask;
				char str_ipv6[INET6_ADDRSTRLEN];

				inet_ntop(family, &(ipv6->sin6_addr), str_ipv6, sizeof(str_ipv6));

				char str[128];
				sprintf(str, "%s/%u", str_ipv6, getPrefix6(&maskv6->sin6_addr));
				interface->address[interface->size] = malloc(sizeof(str));
				strcpy(interface->address[interface->size++], str);
			}
		}
	}

	freeifaddrs(ifaddr);

	unsigned int total = 0;
	for (int i = 0; i < size; i++) {
		total += strlen(interfaces[i]->name);
		total++;

		for (int j = 0; j < interfaces[i]->size; j++) {
			total += strlen("    ");
			total += strlen(interfaces[i]->address[j]);
			total++;
		}
	}

	if (total > 0) {
		char* return_str = malloc(total + 1);
		bzero(return_str, total);
		for (int i = 0; i < size; i++) {
			sprintf(return_str + strlen(return_str), "%s\n", interfaces[i]->name);

			for (int j = 0; j < interfaces[i]->size; j++) {
				sprintf(return_str + strlen(return_str), "    %s\n", interfaces[i]->address[j]);
			}
		}

		return return_str;
	} else {
		return "";
	}
}

void startServer() {
	int server_fd, new_socket, valread;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);

	// Creating socket file descriptor
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
		printf("ifnetshow: socket failed\n");
		exit(EXIT_FAILURE);
	}

	// Forcefully attaching socket to the port 8080
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
		printf("ifnetshow: setsockopt\n");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(PORT);

	// Forcefully attaching socket to the port 8080
	if (bind(server_fd, (struct sockaddr*)&address,
		sizeof(address)) < 0) {
		printf("ifnetshow: bind failed\n");
		exit(EXIT_FAILURE);
	}

	if (listen(server_fd, 3) < 0) {
		printf("ifnetshow: listen\n");
		exit(EXIT_FAILURE);
	}

	printf("[ %ld ] listening on port: %d\n", time(0), PORT);

	while (1) {
		if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
			printf("ifnetshow: accept");
			exit(EXIT_FAILURE);
		}

		char buffer[1024] = { 0 };
		valread = read(new_socket, buffer, 1024);

		if (strcmp(buffer, "EMPTY") == 0) {
			char* ifshow = getAddress(NULL);
			printf("[ %ld ] ifshow -a\n", time(0));
			send(new_socket, ifshow, strlen(ifshow), 0);
		} else {
			char* ifshow = getAddress(buffer);
			printf("[ %ld ] ifshow -i %s\n", time(0), buffer);

			if (strlen(ifshow) == 0) {
				send(new_socket, "EMPTY", strlen("EMPTY"), 0);
			} else {
				send(new_socket, ifshow, strlen(ifshow), 0);
			}
		}

		close(new_socket);
	}
}

void connectServer(const char* serv_ip, const char* interface) {
	int sock = 0, valread;
	struct sockaddr_in serv_addr;
	char buffer[1024] = { 0 };

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("ifnetshow: socket creation error\n");
		exit(EXIT_FAILURE);
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	/* Convert IPv4 and IPv6 addresses from text to binary form */
	if (inet_pton(AF_INET, serv_ip, &serv_addr.sin_addr) <= 0) {
		printf("ifnetshow: invalid address: %s\n", serv_ip);
		exit(EXIT_FAILURE);
	}

	if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("ifnetshow: connection failed: %s\n", serv_ip);
		exit(EXIT_FAILURE);
	}

	if (interface == NULL) {
		send(sock, "EMPTY", strlen("EMPTY"), 0);
	} else {
		send(sock, interface, strlen(interface), 0);
	}

	valread = read(sock, buffer, 1024);

	if (strcmp(buffer, "EMPTY") == 0) {
		printf("ifnetshow: no interface: %s\n", interface);
	} else {
		printf("%s", buffer);
	}

	close(sock);
}

typedef struct arguments {
	char* address;
	char* interface;
} arguments;

static struct option options[] = {
	{"all", no_argument, NULL, 'a'},
	{"help", no_argument, NULL, 'h'},
	{"interface", required_argument, NULL, 'i'},
	{"network", required_argument, NULL, 'n'},
	{"start", no_argument, NULL, 's'}
};

void help() {
	printf("Usage: ifnetshow [ COMMAND ] [ OPTION ]\n");
	printf("Where COMMAND := { -s[tart] | -n[etwork] address }\n");
	printf("      OPTION  := { -a[ll]   | -i[nterface] ifname }\n");
}

arguments* getArgs(int argc, char** argv) {
	char opt;
	int n = 0;
	arguments* args = malloc(sizeof(arguments));

	while ((opt = getopt_long(argc, argv, ":sn:ahi:", options, NULL)) != -1) {
		switch (opt) {
			case 's':
				args->address = NULL;
				break;
			case 'n':
				args->address = optarg;
				break;
			case 'a':
				args->interface = NULL;
				break;
			case 'i':
				args->interface = optarg;
				break;
			case 'h':
				help();
				exit(EXIT_SUCCESS);
				break;
			case ':':
			case '?':
				help();
				exit(EXIT_FAILURE);
				break;
		}

		n++;
	}

	if ((args->address == NULL && n != 1) || (args->address != NULL && n != 2)) {
		help();
		exit(EXIT_FAILURE);
	}

	return args;
}

int main(int argc, char** argv) {
	arguments* args = getArgs(argc, argv);

	if (args->address == NULL) startServer();
	else connectServer(args->address, args->interface);

	return EXIT_SUCCESS;
}
