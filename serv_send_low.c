#include <stdio.h>
#include <stdlib.h>
#include <complex.h>
#include <assert.h>
#include <math.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define _XOPEN_SOURCE
typedef short sample_t;

void die(char *s){
    perror(s);
    exit(1);
}

void sample_to_complex(sample_t * s, complex double * x, long n){
    for (long i = 0; i < n; ++i){
        x[i] = s[i];
    }
}

void complex_to_sample(complex double * x, sample_t * s, long n){
    for (long i = 0; i < n; ++i){
        s[i] = creal(x[i]);
    }
}

/* 高速フーリエ変換;
   w は1のn乗根.
   フーリエ変換の場合   偏角 -2 pi / n
   逆フーリエ変換の場合 偏角  2 pi / n
   xが入力でyが出力.
   xも破壊される
 */
void fft_r(complex double * x, 
	complex double * y, 
    long n, 
    complex double w) {
    if (n == 1) { y[0] = x[0]; }
    else {
        complex double W = 1.0; 
        long i;
        for (i = 0; i < n/2; i++) {
          y[i]     =     (x[i] + x[i+n/2]); /* 偶数行 */
          y[i+n/2] = W * (x[i] - x[i+n/2]); /* 奇数行 */
          W *= w;
        }
        fft_r(y,     x,     n/2, w * w);
        fft_r(y+n/2, x+n/2, n/2, w * w);
        for (i = 0; i < n/2; i++) {
          y[2*i]   = x[i];
          y[2*i+1] = x[i+n/2];
        }
    }
}

void fft(complex double * x, 
    complex double * y, 
	long n) {
    long i;
    double arg = 2.0 * M_PI / n;
    complex double w = cos(arg) - 1.0j * sin(arg);
    fft_r(x, y, n, w);
    for (i = 0; i < n; i++) y[i] /= n;
}

void ifft(complex double * y, complex double * x, long n) {
    double arg = 2.0 * M_PI / n;
    complex double w = cos(arg) + 1.0j * sin(arg);
    fft_r(y, x, n, w);
}

int pow2check(long N) {
    long n = N;
    while (n > 1) {
        if (n % 2) return 0;
        n = n / 2;
    }
    return 1;
}

complex double* lowpass(complex double* Y, long n, double f1){
    for(long l = f1 * n / 44100 + 1; l <= n / 2; ++l){
        Y[l] = 0;
        Y[n - l - 1] = 0;
    }
}

complex double* bandpass(complex double* Y, long n, double f1, double f2, int send_size){
    // for(long l = 0; l < f1 * n / 44100; ++l){
    //     Y[l] = 0;
    //     Y[n - l - 1] = 0;
    // }
    // for(long l = f2 * n / 44100 + 1; l <= n / 2; ++l){
    //     Y[l] = 0;
    //     Y[n - l - 1] = 0;
    // }
    complex double * passed = calloc(sizeof(complex double), send_size);
    for (int i = 0; i < send_size; ++i){
        passed[i] = Y[(int)f1 * n / 44100 + i + 1];
    }
    return passed;
    // Y[int(f1 * n / 44100)　+　1] ~ Y[int(f2 * n / 44100)] までを送る
}

float * compression(sample_t * data, long n, double f1, double f2, int send_size){
    complex double X[n];
    complex double Y[n];    

    sample_to_complex(data, X, n);
    fft(X, Y, n);
    complex double* passed = bandpass(Y, n, f1, f2, send_size);

    float * data_send = calloc(sizeof(float), send_size * 2);
    for (int i = 0; i < send_size; ++i){
        data_send[2 * i] = (float)creal(passed[i]);
        data_send[2 * i + 1] = (float)cimag(passed[i]);
    }
    free(passed);
    return data_send;
}

sample_t * unzip(float * data, long n, double f1, double f2, int send_size){
    complex double X[n];
    complex double Y[n]; 
    sample_t * buf = calloc(sizeof(sample_t), n);
    for (int i = 0; i <= (int)f1 * n / 44100; ++i){
        Y[i] = 0;
        Y[n - i - 1] = 0;
    }
    for (int i = 0; i < send_size; ++i){
        Y[(int)f1 * n / 44100 + i + 1] = data[2 * i] + 1.0j * data[2 * i + 1];
        Y[n - (int)f1 * n / 44100 -i - 2] = data[2 * i] - 1.0j * data[2 * i + 1];
    }
    for (int i = (int)f2 * n / 44100 + 1; i < n / 2; ++i){
        Y[i] = 0;
        Y[n - i - 1] = 0;
    }
    ifft(Y, X, n);
    complex_to_sample(X, buf, n);
    return buf;
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

    const char* command_rec = "rec -t raw -b 16 -c 1 -e s -r 44100 -";
    const char* command_play = "play -t raw -b 16 -c 1 -e s -r 44100 -";

    FILE* fp_rec = popen(command_rec, "r");
    FILE* fp_play = popen(command_play, "w");

    const int N = 4096;
    const double f_max = 22050;
    const double f_min = 0;
    int send_size = (int)f_max * N / 44100 - (int)f_min * N / 44100;

    sample_t data_time[N];
    // complex double * X = calloc(sizeof(complex double), N);
    // complex double * Y = calloc(sizeof(complex double), N);
    //const double f1 = 6000;
    while (1){
        int n_send = fread(data_time, sizeof(sample_t), N, fp_rec);
        if (n_send == -1) {die("fread");}
        if (n_send == 0) {break;}
        float * data_send = compression(data_time, N, f_min, f_max, send_size);//正確にはNではなく、n_send

        send(s, data_send, send_size * 2 * sizeof(float), 0);
        free(data_send);
    }
    shutdown(s, SHUT_WR);
    return 0;
}
