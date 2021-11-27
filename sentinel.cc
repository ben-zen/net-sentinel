// Copyright Ben Lewis, 2021.

#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netdb.h>
#include <memory>
#include <optional>
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

std::optional<sa_family_t> string_to_family(const char *string) {
  return {};
}

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

std::optional<sockaddr> find_interface(int socket_fd, const char *if_name, sa_family_t family) {

  ifaddrs *temp_addrs_list = nullptr;
  if (getifaddrs(&temp_addrs_list) < 0) {
    perror("getifaddrs");
    return {};
  }
  std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> addrs_list(temp_addrs_list, &freeifaddrs);
  temp_addrs_list = nullptr; // No need for this one now.
  
  ifaddrs *cursor = addrs_list.get(); // Cursor
  while (cursor != nullptr) {
    if (cursor->ifa_name
        && (strcmp(if_name, cursor->ifa_name) == 0)) {
      auto family = cursor->ifa_addr->sa_family;
      printf("%s\tfamily: %s\n",
             cursor->ifa_name, family_to_string(family));

      if (family == AF_INET || family == AF_INET6) {
        char host[NI_MAXHOST];
        if (getnameinfo(
              cursor->ifa_addr,
              (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6), host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST) < 0) {
          perror("getnameinfo");
          return {};
        }
        printf("\taddress: <%s>\n", host);
        return {*(cursor->ifa_addr)};
      }
    }
  cursor = cursor->ifa_next;
  }
  printf("Interface not found: %s\n", if_name);
  return {};
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

    find_interface(socket_fd, argv[1], AF_INET);

    if (socket_fd) {
        if (close(socket_fd) < 0) {
            perror("close failed on socket");
        }
    }
}