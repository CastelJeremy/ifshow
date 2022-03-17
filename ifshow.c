#include <arpa/inet.h>
#include <getopt.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

	//*Parse through every bits of the address. */
	while (((sa->s6_addr[i] >> b) & 1) > 0 && i < 16) {
		prefix++;
		b = (b + 1) % 8;

		if (b == 0) i++;
	}

	return prefix;
}

typedef struct arguments {
	char* interface;
} arguments;

static struct option options[] = {
	{"all", no_argument, NULL, 'a'},
	{"help", no_argument, NULL, 'h'},
	{"interface", required_argument, NULL, 'i'}
};

void help() {
	printf("Usage: ifshow [ OPTION ]\n");
	printf("Where OPTION := { -a[ll] | -i[nterface] ifname }\n");
}

arguments* getArgs(int argc, char** argv) {
	char opt;
	int n = 0;
	arguments* args = malloc(sizeof(arguments));

	while ((opt = getopt_long(argc, argv, ":ahi:", options, NULL)) != -1) {
		switch (opt) {
			case 'a':
				args->interface = "";
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

	if (n != 1) {
		help();
		exit(EXIT_FAILURE);
	}

	return args;
}

int main(int argc, char** argv) {
	/* Used to check for specific interface name. */
	arguments* args = getArgs(argc, argv);

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

		if (strcmp(args->interface, "") != 0 && strcmp(args->interface, ifa->ifa_name) != 0)
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

	/* Display interfaces and addresses lists. */
	for (int i = 0; i < size; i++) {
		printf("%s \n", interfaces[i]->name);

		for (int j = 0; j < interfaces[i]->size; j++) {
			printf("    %s \n", interfaces[i]->address[j]);
		}
	}

	freeifaddrs(ifaddr);
	exit(EXIT_SUCCESS);
}