// SSRF address classifier self-check.
// Build after the module is compiled (inside the build container, from the nginx source dir):
//   g++ -o /tmp/t ../tests/test_is_blocked_addr.cpp objs/addon/src/ngx_http_morph_utils.o -lcrypto && /tmp/t
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cassert>
#include <cstdio>
#include <cstring>

namespace MorphUtils { bool is_blocked_addr(const struct sockaddr *sa); }

static bool blocked(const char *ip) {
    struct sockaddr_storage ss;
    memset(&ss, 0, sizeof(ss));
    if (strchr(ip, ':')) {
        ss.ss_family = AF_INET6;
        assert(inet_pton(AF_INET6, ip, &((struct sockaddr_in6 *)&ss)->sin6_addr) == 1);
    } else {
        ss.ss_family = AF_INET;
        assert(inet_pton(AF_INET, ip, &((struct sockaddr_in *)&ss)->sin_addr) == 1);
    }
    return MorphUtils::is_blocked_addr((struct sockaddr *)&ss);
}

int main() {
    const char *bad[] = {"0.0.0.0", "10.1.2.3", "100.64.0.1", "127.0.0.1", "169.254.169.254",
                         "172.16.0.1", "172.31.255.255", "192.168.0.1", "192.0.0.8", "198.18.0.1",
                         "224.0.0.1", "255.255.255.255",
                         "::", "::1", "::ffff:127.0.0.1", "::ffff:10.0.0.1", "64:ff9b::a9fe:a9fe",
                         "fc00::1", "fd12::1", "fe80::1", "ff02::1",
                         "::ffff:0:7f00:1", "64:ff9b:1::1", "2002:7f00:1::1", "2002:a9fe:a9fe::1",
                         "2001:0:4136:e378::1", "fec0::1"};
    const char *good[] = {"8.8.8.8", "1.1.1.1", "172.15.0.1", "172.32.0.1", "100.128.0.1",
                          "192.169.0.1", "11.0.0.1", "2606:4700::1111", "::ffff:8.8.8.8",
                          "2002:808:808::1", "2001:4860:4860::8888"};
    for (const char *ip : bad)  { if (!blocked(ip)) { printf("FAIL should block %s\n", ip); return 1; } }
    for (const char *ip : good) { if (blocked(ip))  { printf("FAIL should allow %s\n", ip); return 1; } }
    printf("ok\n");
    return 0;
}
