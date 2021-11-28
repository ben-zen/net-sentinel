// Copyright Ben Lewis, 2021.

#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <memory>
#include <optional>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>

class socket_t {
public:
  socket_t(int domain, int type, int protocol) {
    _fd = socket(domain, type, protocol);
    if (_fd <= 0) {
      perror("Failed opening a socket.");
    }
  }
  ~socket_t() {
    if (_fd > 0) {
      puts("Closing socket.");
      close(_fd);
    }
  }

  operator int() {
     return _fd;
  }

private:
  int _fd;
};

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
        sockaddr addr = {};
        addr.sa_family = AF_INET;
        ((sockaddr_in *)(&addr))->sin_addr = ((sockaddr_in *)(cursor->ifa_addr))->sin_addr;
        return {addr};
      }
    }
  cursor = cursor->ifa_next;
  }
  printf("Interface not found: %s\n", if_name);
  return {};
}

// Adapted from the RFC.
uint16_t compute_checksum(uint8_t *data, size_t len) {
  uint32_t acc = 0;
  size_t count = len;
  while (count > 1) {
    acc += * (uint16_t *)data;
    data += 2;
    count -= 2;
  }

  if (count > 0) {
    acc += *data;
  }

  while (acc >> 16) {
    acc = (acc & 0xFFFF) + (acc >> 16);
  }

  return ~acc;
}

#pragma pack(push,1)
struct echo_request {
  icmphdr header {};
  uint32_t timestamp;
  char buffer[64] = {};
  echo_request() {
    header.type = ICMP_ECHO;
    header.un.echo.id = getpid();
    header.un.echo.sequence = 1;
    struct timeval val{};
    gettimeofday(&val, nullptr);
    timestamp = (val.tv_usec % 8460000) / 100;
  }
};

struct echo_response {
    iphdr header;
    echo_request request;
    char filler[256];
  };
#pragma pack(pop)

int main(int argc, char *argv[]) {

  socket_t read_socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  if (read_socket <= 0) {
    perror("socket failed");
    return 1;
  }

    if (argc == 1) {
        puts("Insufficient arguments!");
        return 2;
    }

  // Find an interface matching the supplied name
  auto addr = find_interface(read_socket, argv[1], AF_INET);
  if (!addr) {
    printf("Failed to find interface %s\n", argv[1]);
    return 3;
  }

  auto *addr_v4 = (sockaddr_in *)&addr.value();
  addr_v4->sin_port = IPPROTO_ICMP;
  uint8_t address[4]{};
  memcpy(address, &addr_v4->sin_addr.s_addr, sizeof(address));
  printf("Returned struct: %d %x %d.%d.%d.%d\n", addr_v4->sin_family, addr_v4->sin_port, address[0], address[1], address[2], address[3]);

  // Bind to the interface!
  if (bind(read_socket, &(addr.value()), sizeof(sockaddr_in)) ==-1) {
    perror("failed to bind to socket with supplied sockaddr");
    return 4;
  }

  puts("Bound to socket! Ready to listen?");

  echo_request request{};
  char example[] = "test icmp echo impl";
  memcpy(request.buffer, example, sizeof(example));
  request.header.checksum = compute_checksum((uint8_t *) &request, sizeof(request));

  sockaddr_in destination{};
  destination.sin_family = AF_INET;
  destination.sin_port = IPPORT_ECHO;
  destination.sin_addr.s_addr = 0x08080808; // 8.8.8.8

  if (sendto(read_socket, &request, sizeof(request), 0, (const sockaddr *)&destination, sizeof(destination)) == -1) {
    perror("sendto failed.");
    return 5;
  }

  fd_set fds{};
  FD_ZERO(&fds);
  FD_SET(read_socket, &fds);
  timeval timeout_duration{};
  timeout_duration.tv_sec = 15;
  if (select(1, &fds, nullptr, nullptr, &timeout_duration) == -1) {
    perror("Error waiting to read from socket.");
    return 6;
  }

  echo_response response;

  if (recvfrom(read_socket, &response, sizeof(response), 0, nullptr, nullptr) == -1) {
    perror("recvfrom failed");
    return 7;
  }

  printf("response has type %d, TTL of %d", response.request.header.type, response.header.ttl);

  return 0;
}