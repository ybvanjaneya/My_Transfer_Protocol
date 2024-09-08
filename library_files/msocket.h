#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <pthread.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <signal.h>



#define p (0.05)
#define T 5

// Custom socket type for MTP
#define SOCK_MTP 3
#define MAX_SOCKETS 25
#define SEND_BUF_SIZE 10
#define RECV_BUF_SIZE 5
#define MSG_SIZE 1024
#define FRAME_SIZE 1033
#define SEQ 16

// errors
#define ENOTBOUND 99

typedef struct sbuf {
    char msg[MSG_SIZE];
    struct timeval sent_time;
    int sent;
    int seq;
    int occupied;
}sbuf;
typedef struct SEND_B {
    int recv_buf_size;
    int last_ack_seq;
    int tot_msgs;
    int tot_sends;
    sbuf send_buffer[SEND_BUF_SIZE];
}SEND_B;

typedef struct rbuf {
    char msg[MSG_SIZE];
    struct timeval recv_time;
    int occupied;
    int seq;
    int recvd;
}rbuf;
typedef struct RECV_B {
    int last_inorder_seq;
    int flag_nospace;
    rbuf recv_buffer[RECV_BUF_SIZE];
    rbuf outoforder[RECV_BUF_SIZE];
}RECV_B;
typedef struct swnd {
    int send_wnd_size;
    int start_index;
    int sequence[SEND_BUF_SIZE]; // msgs that are sent but not yet acknowledged
}swnd;
typedef struct rwnd{
    int recv_wnd_size;
    int start_index;
    int sequence[RECV_BUF_SIZE]; // msgs that are received but not yet acknowledged
}rwnd;

typedef struct mtp_socket_info {
    int alloted;
    pid_t pid;
    int udp_sock_id;
    struct sockaddr_in other;
    SEND_B send_buf;
    RECV_B recv_buf;
    swnd send_window;
    rwnd recv_window;
}mtp_socket_info;
typedef struct sock_info {
    int sock_id;
    char ip[INET_ADDRSTRLEN];
    int port;
    int err;
}sock_info;
// Global error variable
extern int m_errno;

// Function prototypes
int m_socket(int domain, int type, int protocol);
int m_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen, const struct sockaddr* dest, socklen_t destlen );
int m_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *addr, socklen_t addrlen);
int m_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *addr, socklen_t *addrlen);
int m_close(int sockfd);
int dropMessage(float);
void set_curr_time(struct timeval*);
int min(int x, int y);
int max(int x, int y);
void convertSeqToStr(int n, char * );
int convertStrToSeq(char *);

void semaphore_wait(int sem);
void semaphore_signal(int sem);