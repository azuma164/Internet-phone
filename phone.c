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
    if (argc != 2 && argc != 3) {
        die("usage:\nserver: ./phone port_num\nclient: ./phone IP_addr port_num");
    }
    
    // server setting
    if (argc == 2){
        const int port_num = atoi(argv[1]);
        
        // make socket 
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

        const char* command_rec = "rec -t raw -b 16 -c 1 -e s -r 44100 -";
        const char* command_play = "play -t raw -b 16 -c 1 -e s -r 44100 -";
        FILE* fp_rec = popen(command_rec, "r");
        FILE* fp_play = popen(command_play, "w");

        const int N = 256;
        short data_send[N];
        short data_recv[N];
        while (1){
            int n_send = fread(data_send, sizeof(short), N, fp_rec);
            if (n_send == -1) {die("fread");}
            if (n_send == 0) {break;}
            send(s, data_send, n_send * 2, 0);

            int n_recv = recv(s, data_recv, N * 2, 0);
            if (n_recv == -1) {die("recv");}
            if (n_recv == 0) {break;}
            fwrite(data_recv, sizeof(short), n_recv, fp_play);

        }

        shutdown(s, SHUT_WR);
    }
    // client setting
    else {
        const char* IP_addr = argv[1];
        const int port_num = atoi(argv[2]);

        // make socket
        int s = socket(PF_INET, SOCK_STREAM, 0);

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        int t =inet_aton(IP_addr, &addr.sin_addr);
        if (t == 0) {die("inet_aton");}
        addr.sin_port = htons(port_num);

        int ret = connect(s, (struct sockaddr *)&addr, sizeof(addr));
        if (ret == -1) {die("connect");}

        const char* command_rec = "rec -t raw -b 16 -c 1 -e s -r 44100 -";
        const char* command_play = "play -t raw -b 16 -c 1 -e s -r 44100 -";
        FILE* fp_rec = popen(command_rec, "r");
        FILE* fp_play = popen(command_play, "w");

        const int N = 256;
        short data_send[N];
        short data_recv[N];
        while (1){
            int n_send = fread(data_send, sizeof(short), N, fp_rec);
            if (n_send == -1) {die("fread");}
            if (n_send == 0) {break;}
            send(s, data_send, n_send * 2, 0);

            int n_recv = recv(s, data_recv, N * 2, 0);
            if (n_recv == -1) {die("recv");}
            if (n_recv == 0) {break;}
            fwrite(data_recv, sizeof(short), n_recv / 2, fp_play);

        }

        shutdown(s, SHUT_WR);
    }

    // finish call
    printf("\nCall ended.\n");
    return 0;
}   
