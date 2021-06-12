#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

void die(char *s){
    perror(s);
    exit(1);
}
 
int main(int argc, char **argv){
    if (argc != 3) {die("usage: %s IP_addr port_num");}
    const char* IP_addr = argv[1];
    const int port_num = atoi(argv[2]);

    int s = socket(PF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    int t =inet_aton(IP_addr, &addr.sin_addr);
    if (t == 0) {die("inet_aton");}
    addr.sin_port = htons(port_num);
    
    int ret = connect(s, (struct sockaddr *)&addr, sizeof(addr));
    if (ret == -1) {die("connect");}

    const int N = 256;
    char data[N];
    while (1){
        int n = recv(s, data, N, 0);
        if (n == -1) {die("recv");}
        if (n == 0) {break;}
        
        write(1, data, n);
    }

    shutdown(s, SHUT_WR);
    return 0;
}
