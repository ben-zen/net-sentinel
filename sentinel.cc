#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netdb.h>
#include <memory>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct Ifconf : ifconf {
  Ifconf() {
      ifc_len = 0;
      ifc_req = nullptr;
  }
  ~Ifconf() {
      if (ifc_req) {
          delete[] ifc_req; // This can be improved
      }
  }
};

const char *family_to_string(int sa_family) {
    switch (sa_family) {
        case AF_PACKET:
        return "AF_PACKET";
        case AF_INET:
        return "AF_INET";
        case AF_INET6:
        return "AF_INET6";
    }
    return "UNKNOWN";
}

void find_interface(int socket_fd, const char *if_name) {
    ifaddrs *addrs_list = nullptr;
    ifaddrs *cursor = nullptr;

    if (getifaddrs(&addrs_list) < 0) {
        perror("getifaddrs");
        return;
    }

    cursor = addrs_list;
    while (cursor != nullptr) {
        if (cursor->ifa_name
            && (strcmp(if_name, cursor->ifa_name) == 0)) {
            auto family = cursor->ifa_addr->sa_family;
            printf("%s\tfamily: %s\n",
                cursor->ifa_name, family_to_string(family));

            if (family == AF_INET || family == AF_INET6) {
                char host[NI_MAXHOST];
                if (getnameinfo(cursor->ifa_addr, (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6), host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST) < 0) {
                    perror("getnameinfo");
                    return;
                }
                printf("\taddress: <%s>\n", host);
            }
            // Once we've found the target interface,
            // we can ... return it? I need to figure out how I'll hand
            // that along.
        }
        cursor = cursor->ifa_next;
    }
    freeifaddrs(addrs_list);

}

int main(int argc, char *argv[]) {
    // Find an interface matching the supplied name
    int socket_fd = 0;
    socket_fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (socket_fd <= 0) {
        perror("socket failed");
        return 1;
    }

    if (argc == 1) {
        puts("Insufficient arguments!");
        return 2;
    }

    find_interface(socket_fd, argv[1]);

    if (socket_fd) {
        if (close(socket_fd) < 0) {
            perror("close failed on socket");
        }
    }
}