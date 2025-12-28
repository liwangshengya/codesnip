
### 利用域名获取IP地址
原理：利用DNS域名解析，获取IP地址
函数：gethostbyname
``` c++
#include <netdb.h>
struct hostent * gethostbyname(const char * hostname);
```
该函数已经弃用，请使用getaddrinfo函数
``` c++
int getaddrinfo(const char *restrict node,
                       const char *restrict service,
                       const struct addrinfo *restrict hints,
                       struct addrinfo **restrict res);
/*
node: 主机名，如 "www.example.com" 或 IP 地址字符串 "127.0.0.1"。
service: 服务名（如 "http"）或十进制端口号（如 "8080"）。
hints: 一个你填写的 addrinfo 结构体，用来过滤结果。
res: 一个指向指针的指针，函数会把结果链表的头地址存到这里。

void freeaddrinfo(struct addrinfo *res);：用于释放 getaddrinfo 动态分配的内存。必须调用！
*/
```

### 利用IP地址来获取域名
原理：反向DNS解析，获取域名

- 反向DNS查询 的基本原理

`gethostbyaddr` 的功能，本质上执行的是一个**反向DNS查询**。它并不是一个“神奇”的数据库，而是一个标准化的、有章可循的流程。

我们知道，普通的DNS查询（正向查询）是：**域名 → IP地址**。这通过查询域名的 `A` (IPv4) 或 `AAAA` (IPv6) 记录来实现。

反向查询则正好相反：**IP地址 → 域名**。它通过查询一种特殊的DNS记录来实现，这种记录叫做 **PTR 记录**。

**工作流程如下：**

1.  **IP地址的“反转”**：为了能将IP地址放入DNS的层级结构中，需要将其“反转”并附加一个特殊的顶级域。
    *   对于 **IPv4**：将IP地址的四个八位字节顺序反转，然后加上 `.in-addr.arpa`。
        *   例如，要查询IP `8.8.8.8`，系统会构造一个查询域名：`8.8.8.8.in-addr.arpa`。
        *   再比如，查询 `1.2.3.4`，会查询 `4.3.2.1.in-addr.arpa`。
    *   对于 **IPv6**：过程更复杂，是将128位的地址每4位（一个nibble）反转，然后用 `.` 连接，最后加上 `.ip6.arpa`。

2.  **查询PTR记录**：系统拿着这个构造好的特殊域名（如 `8.8.8.8.in-addr.arpa`），像查询普通DNS一样，向DNS服务器请求它的 **PTR记录**。

3.  **返回结果**：如果该IP的管理者为其配置了PTR记录，DNS服务器就会返回这个记录的值，这个值就是一个域名。`gethostbyaddr` 函数将这个域名返回给你。

**一个实际的例子：**

你可以在命令行里用 `nslookup` 或 `dig` 工具来模拟这个过程：

```bash
# 查询 8.8.8.8 的反向DNS
nslookup 8.8.8.8

# 服务器会返回：
Non-authoritative answer:
8.8.8.8.in-addr.arpa     name = dns.google.
```

这里，`dns.google` 就是 Google 为其公共DNS服务器 `8.8.8.8` 配置的PTR记录。

---

- CDN场景下的反向DNS：为什么查不到源站域名？


**结论先行：当对一个CDN节点的IP地址进行反向查询时，几乎总是会得到CDN服务商自己为该服务器配置的域名，而不会得到最终用户访问的网站域名。**

**为什么呢？**

1.  **一对多关系**：这是最根本的原因。一个CDN的边缘服务器IP地址，可能同时为成千上万个不同的网站提供加速服务（比如 `news.a.com`, `shop.b.com`, `video.c.com` 都可能解析到同一个IP `1.2.3.4`）。反向DNS查询只能返回**一个**域名，它根本不可能知道你当时访问的是哪个网站。因此，它无法返回源站域名。

2.  **所有权与管理权**：IP地址是资源，它被分配给了CDN服务商（如Cloudflare, Akamai, AWS CloudFront）。因此，只有CDN服务商才有权限为这个IP地址配置PTR记录。他们会怎么配置呢？他们会配置一个对自己运维有意义的名字，比如：
    *   `ec2-1-2-3-4.compute.amazonaws.com` (AWS)
    *   `1-2-3-4.customer.cloudflare.com` (Cloudflare)
    *   `a1-2-3-4.deploy.akamaitechnologies.com` (Akamai)
    这些名字有助于CDN服务商自己管理和诊断他们的服务器集群。

3.  **动态性与负载均衡**：CDN会根据你的地理位置、服务器负载、网络状况等因素，动态地为你分配一个“最佳”的边缘节点IP。你这次访问 `www.example.com` 得到的是IP `A`，下次可能就是IP `B`。如果反向DNS要返回 `www.example.com`，那么CDN服务商需要为成千上万个IP都配置指向 `www.example.com` 的PTR记录，这不仅不现实，也违背了CDN的动态性原则。


---


函数：gethostbyaddr
``` c++
 struct hostent *gethostbyaddr(const void addr[.len],
                               socklen_t len, int type);
/*
arg:
    addr  含有IP地址信息的in_addr结构体指针。为了同时传递IPv4地址之外的其他息，该变量的类 型声明为char指针。 
    len 向第一个参数传递的地址信息的字节数，IPv4时为4，IPv6时为16。 
    family 传递地址族信息，IPv4时为AF_INET，IPv6时为AF_INET6。 
return:
    成功返回一个指向hostent结构体的指针，失败返回NULL。
*/                                                  
```
该函数已经弃用，请使用`getnameinfo`函数
``` c++
#include <netdb.h>

int getnameinfo(const struct sockaddr *sa, socklen_t salen,
                char *host, socklen_t hostlen,
                char *serv, socklen_t servlen,
                int flags);
/*
参数详解：

sa: 指向你要转换的 sockaddr 结构体的指针。
salen: sockaddr 结构体的大小（例如 sizeof(struct sockaddr_in)）。
host: 一个你提供的缓冲区，用于存储返回的主机名。
hostlen: host 缓冲区的大小。
serv: 一个你提供的缓冲区，用于存储返回的服务名或端口号。
servlen: serv 缓冲区的大小。
flags: 控制标志，非常关键。

常用 flags 选项：
NI_NOFQDN: 只返回主机名部分，不返回完整的域名（例如，返回 my-laptop 而不是 my-laptop.localdomain）。
NI_NUMERICHOST: 不进行DNS反向查询，直接返回IP地址的字符串形式（例如 "127.0.0.1"）。这可以避免网络延迟。
NI_NUMERICSERV: 不进行服务名查询，直接返回端口号的字符串形式（例如 "8080"）。
NI_NAMEREQD: 如果主机名无法解析，则返回一个错误。默认情况下，如果解析失败，它会返回IP地址字符串。*/ 
```




