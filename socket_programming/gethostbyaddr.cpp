#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <netdb.h>
 void error_handling(std::string message) {
    std::cout << message << std::endl;
    exit(1);
}

int main(int argc, char *argv[])
{
  struct hostent *host;
  struct sockaddr_in addr;
  
  if(argc != 2) {
      std::cout << "Usage : " << argv[0] << " <IP address>" << std::endl;
      exit(1);
  }


  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  /* use inet_aton to validate and convert the textual IP */
  if (inet_aton(argv[1], &addr.sin_addr) == 0) {
      error_handling(std::string("Invalid IP address: ") + argv[1]);
  }

  host = gethostbyaddr((const void*)&addr.sin_addr, sizeof(addr.sin_addr), AF_INET);
  if (!host) {
      /* print resolver error information */
      herror("gethostbyaddr");

      /* try a getnameinfo fallback to see if reverse lookup via resolver works differently */
      char hostbuf[NI_MAXHOST];
      int rc = getnameinfo((struct sockaddr*)&addr, sizeof(addr), hostbuf, sizeof(hostbuf), NULL, 0, NI_NAMEREQD);
      if (rc == 0) {
          std::cout << "Reverse lookup (getnameinfo) succeeded: " << hostbuf << std::endl;
      } else {
          std::cout << "Reverse lookup failed: " << gai_strerror(rc) << std::endl;
          std::cout << "Numeric address: " << inet_ntoa(addr.sin_addr) << std::endl;
      }
      error_handling("gethostbyaddr() error");
  }
  std::cout << "Official name: " << host->h_name << std::endl;

  for(int i = 0; host->h_aliases[i]; i++) {
      std::cout << "Aliases: " << host->h_aliases[i] << std::endl;
  }
  std::cout << "Address type: " << (host->h_addrtype == AF_INET ? "AF_INET" : "AF_INET6") << std::endl;

  for(int i = 0; host->h_addr_list[i]; i++) {
      std::cout << "IP Address: " << inet_ntoa(*(struct in_addr*)host->h_addr_list[i]) << std::endl;
  }

  return 0;


}


