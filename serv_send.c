#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define _XOPEN_SOURCE

void die(char *s){
    perror(s);
    exit(1);
}
 
int main(int argc, char **argv){
    if (argc != 2) {die("usage: %s port_num");}
    const int port_num = atoi(argv[1]);

    int ss = socket(PF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_num);
    
    int ret = bind(ss, (struct sockaddr *)&addr, sizeof(addr));
    if (ret == -1) {die("bind");}

    listen(ss, 10);

    struct sockaddr_in client_addr;
    socklen_t len = sizeof(struct sockaddr_in);
    int s = accept(ss, (struct sockaddr *) &client_addr, &len);

    const int N = 256;
    char data[N];
    while (1){
        int n = read(0, data, N);
        if (n == -1) {die("read");}
        if (n == 0) {break;}
        send(s, data, n, 0);
    }

    shutdown(s, SHUT_WR);
    return 0;
}
