#include <errno.h>
#include <net/if.h>
#include <memory>
#include <stdio.h>
#include <sys/ioctl.h>

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

void find_interface(int socket_fd, const char *if_name) {
    Ifconf config{};
    // Make a call to get the buffer to allocate.
    auto err = ioctl(socket_fd, SIOCGIFCONF, &config);
    if (err != 0) {
        perror("ioctl: SIOCGIFCONF for buffer length");
    }
    
    config.ifc_ifcu.ifcu_buf = new char[config.ifc_len];
    err = ioctl(socket_fd, SIOCGIFCONF, &config);
    if (err != 0) {
        perror("ioctl SIOCGIFCONF for interfaces");
    }

    // Now find the actual interface!
    auto len = config.ifc_len / sizeof(struct ifreq);
    // for now, we'll loop over the whole thing
    for (int i = 0; i < len; i++) {
        puts(config.ifc_ifcu.ifcu_req[i].ifr_ifrn.ifrn_name);
    }
}

int main(int argc, char *argv[]) {
    // Find an interface matching the supplied name
    int socket_fd;
    socket_fd = socket(AF_ROUTE, SOCK_RAW, 0);
    if (socket_fd == 0) {
        // error! 
        return 1;
    }

    if (argc == 1) {
        puts("Insufficient arguments!");
    }

    find_interface(socket_fd, argv[1]);
}