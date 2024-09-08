#include "msocket.h"

int m_socket(int domain, int type, int protocol){
    int shm_SOCK_INFO = shmget(3500, sizeof(sock_info), IPC_CREAT|0777);
    sock_info*SOCK_INFO = (sock_info*)shmat(shm_SOCK_INFO, 0, 0 );
    int shm_SM = shmget(3501, sizeof(mtp_socket_info)*MAX_SOCKETS, 0777|IPC_CREAT);
    mtp_socket_info*SM = (mtp_socket_info*) shmat(shm_SM, 0, 0);
    int sem1, sem2;
    sem1 = semget(4100, 1, 0777|IPC_CREAT);
    sem2 = semget(4101, 1, 0777|IPC_CREAT);
    struct sembuf sem1_op;
    struct sembuf sem2_op;
    sem1_op.sem_flg = 0;
    sem1_op.sem_num = 0;
    sem2_op.sem_flg = 0;
    sem2_op.sem_num = 0;
    int mutex;
    mutex = semget(5000, 1, 0777|IPC_CREAT);
    struct sembuf mtx_op;
    mtx_op.sem_flg = 0;
    mtx_op.sem_num = 0;

    int return_value = -1;
    int i=0;
    mtx_op.sem_op = -1;
    semop(mutex, &mtx_op, 1);
    for (i=0; i<MAX_SOCKETS; i++){
        if (SM[i].alloted == 0){
            // not alloted

            //printf("[%d]\n", i);
            sem1_op.sem_op = 1;
            semop(sem1, &sem1_op, 1);   // signal sem1
            //semaphore_signal(mutex);
            sem2_op.sem_op = -1;
            semop(sem2, &sem2_op, 1);   // wait sem2

            //semaphore_wait(mutex);
            
            if (SOCK_INFO->sock_id == -1){
                // error
                errno = SOCK_INFO->err;
                return_value = -1;
            }
            else {
                printf("mtp:[%d]-udp socket alloted %d\n", i, SOCK_INFO->sock_id);
                SM[i].alloted = 1;
                return_value = i;
                SM[i].pid = getpid();
                SM[i].udp_sock_id = SOCK_INFO->sock_id;
                printf("alloted: %d udp: %d pid: %d\n", SM[i].alloted, SM[i].udp_sock_id, SM[i].pid);
            }
            
            break;
        }
    }
   
    if (i == MAX_SOCKETS){
        printf("m_socket: No free mtp scoket available\n");
        errno = ENOBUFS;
        return_value = -1;
    }
    SOCK_INFO->err=0;
    SOCK_INFO->port = 0;
    SOCK_INFO->sock_id = 0;
    memset(SOCK_INFO->ip, 0, INET_ADDRSTRLEN);
    mtx_op.sem_op = 1;
    semop(mutex, &mtx_op, 1);
    shmdt(SOCK_INFO);
    shmdt(SM);
    return return_value;
}
int m_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen, const struct sockaddr* dest, socklen_t destlen ){
    int shm_SOCK_INFO = shmget(3500, sizeof(sock_info), IPC_CREAT|0777);
    sock_info*SOCK_INFO = (sock_info*)shmat(shm_SOCK_INFO, 0, 0 );
    int shm_SM = shmget(3501, sizeof(mtp_socket_info)*MAX_SOCKETS, 0777|IPC_CREAT);
    mtp_socket_info*SM = (mtp_socket_info*) shmat(shm_SM, 0, 0);
    int sem1, sem2;
    sem1 = semget(4100, 1, 0777|IPC_CREAT);
    sem2 = semget(4101, 1, 0777|IPC_CREAT);
    struct sembuf sem1_op;
    struct sembuf sem2_op;
    sem1_op.sem_flg = 0;
    sem1_op.sem_num = 0;
    sem2_op.sem_flg = 0;
    sem2_op.sem_num = 0;
    int mutex;
    mutex = semget(5000, 1, 0777|IPC_CREAT);
    struct sembuf mtx_op;
    mtx_op.sem_flg = 0;
    mtx_op.sem_num = 0;

    int return_value = -1;
    mtx_op.sem_op = -1;
    semop(mutex, &mtx_op, 1);

    if (sockfd >= MAX_SOCKETS){
        // no such mtp socket is there
        printf("m_bind: No such mtp socket created\n");
        errno = EBADF;
    }
    else {
        int udp_socket = SM[sockfd].udp_sock_id;
        const struct sockaddr_in *addr_in = (const struct sockaddr_in *) addr;
        int port = ntohs(addr_in->sin_port);
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(addr_in->sin_addr), ip, INET_ADDRSTRLEN);
        SOCK_INFO->port = port;
        SOCK_INFO->sock_id = udp_socket;
        sprintf(SOCK_INFO->ip, "%s", ip);

        sem1_op.sem_op = 1;
        semop(sem1, &sem1_op, 1);   // signal sem1
        sem2_op.sem_op = -1;
        semop(sem2, &sem2_op, 1);   // wait sem2

        if (SOCK_INFO->sock_id == -1){
            return_value = -1; // error
            errno = SOCK_INFO->err;
        }
        else {
            return_value = 0; // success
            

            SM[sockfd].other = *(struct sockaddr_in *) dest;

            
        }
    }

    SOCK_INFO->err=0;
    SOCK_INFO->port = 0;
    SOCK_INFO->sock_id = 0;
    memset(SOCK_INFO->ip, 0, INET_ADDRSTRLEN);
    mtx_op.sem_op = 1;
    semop(mutex, &mtx_op, 1);
    shmdt(SOCK_INFO);
    shmdt(SM);
    return return_value;
}
int m_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *addr, socklen_t addrlen){
    int shm_SM = shmget(3501, sizeof(mtp_socket_info)*MAX_SOCKETS, 0777|IPC_CREAT);
    mtp_socket_info*SM = (mtp_socket_info*) shmat(shm_SM, 0, 0);
    int mutex;
    mutex = semget(5000, 1, 0777|IPC_CREAT);
    struct sembuf mtx_op;
    mtx_op.sem_flg = 0;
    mtx_op.sem_num = 0;

    int return_value = -1;
    struct sockaddr_in dest = *(struct sockaddr_in *)addr;
    mtx_op.sem_op = -1;
    semop(mutex, &mtx_op, 1);
    if (sockfd <0 || sockfd>=MAX_SOCKETS){
        return_value = -1;
        errno = EBADF;
        printf("invalid socket id\n");
    }
    else if (SM[sockfd].alloted != 1){
        return_value = -1;
        errno = EBADF;
        printf("invalid socket id\n");
    }
    else if (SM[sockfd].other.sin_addr.s_addr== dest.sin_addr.s_addr && SM[sockfd].other.sin_port==dest.sin_port){
        // bound
        if (SM[sockfd].send_window.send_wnd_size < SEND_BUF_SIZE){
            // space is there
            int index = (SM[sockfd].send_window.start_index + SM[sockfd].send_window.send_wnd_size)%SEND_BUF_SIZE;
            int i=index;
            //while(1){
                /*
                if ( index>=SM[sockfd].send_window.start_index ){
                    if (i<SM[sockfd].send_window.start_index || i>=index){

                    }
                    else {
                        break;
                    }
                }
                if ( index<=SM[sockfd].send_window.start_index){
                    if (i<SM[sockfd].send_window.start_index && i>=index){

                    }
                    else {
                        break;
                    }
                }*/
                if (SM[sockfd].send_buf.send_buffer[i].occupied==0){
                    // means a space for the message
                    memset(SM[sockfd].send_buf.send_buffer, 0, min(len, MSG_SIZE));
                    strcpy(SM[sockfd].send_buf.send_buffer[index].msg, (char *)buf);
                    //printf("sendto success\n");
                    SM[sockfd].send_buf.send_buffer[index].occupied = 1;
                    SM[sockfd].send_buf.send_buffer[index].sent = 0;
                    SM[sockfd].send_buf.send_buffer[index].seq = (SM[sockfd].send_buf.send_buffer[(index-1+SEND_BUF_SIZE)%SEND_BUF_SIZE].seq+1)%SEQ;
                    //SM[sockfd].send_window.send_wnd_size++;
                    //printf("seq num: %d win: %d\n", SM[sockfd].send_buf.send_buffer[index].seq, SM[sockfd].send_window.send_wnd_size);
                    SM[sockfd].send_buf.tot_msgs++;
                    return_value = min(len, MSG_SIZE);
                    //break;
                }
                // i++;
                // i = i%SEND_BUF_SIZE;
            //}
        }
        else  {
            // no space is there in sender buffer
            printf("no buffer\n");
            return_value = -1;
            errno = ENOBUFS;
        }
    }
    else {
        // not bound
        printf("not bound\n");
        return_value = -1;
        errno = ENOTCONN;
    }

    mtx_op.sem_op = 1;
    semop(mutex, &mtx_op, 1);
    shmdt(SM);
    return return_value;
}
int m_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *addr, socklen_t *addrlen){
    int shm_SM = shmget(3501, sizeof(mtp_socket_info)*MAX_SOCKETS, 0777|IPC_CREAT);
    mtp_socket_info*SM = (mtp_socket_info*) shmat(shm_SM, 0, 0);
    int mutex;
    mutex = semget(5000, 1, 0777|IPC_CREAT);
    struct sembuf mtx_op;
    mtx_op.sem_flg = 0;
    mtx_op.sem_num = 0;

    int return_value = -1;

    struct sockaddr_in dest = *(struct sockaddr_in *)addr;
    mtx_op.sem_op = -1;
    semop(mutex, &mtx_op, 1);
    
    int index = (SM[sockfd].recv_window.start_index + SM[sockfd].recv_window.recv_wnd_size)%RECV_BUF_SIZE;
    //printf("enterted recvfrom\n");
    if (sockfd <0 || sockfd>=MAX_SOCKETS){
        return_value = -1;
        errno = EBADF;
        //printf("invalid socket\n");
    }
    else if (SM[sockfd].alloted != 1){
        return_value = -1;
        errno = EBADF;
    }
    else if (SM[sockfd].other.sin_addr.s_addr== dest.sin_addr.s_addr && SM[sockfd].other.sin_port==dest.sin_port){
        int i= SM[sockfd].recv_window.start_index;
        //while(1){
            /*
            if (index>=SM[i].recv_window.start_index){
                if (i<SM[i].recv_window.start_index || i>=index){break;}
                
            }
            if (index<SM[i].recv_window.start_index){
                if (i<SM[i].recv_window.start_index && i>=index){break;}
                
            }
            */
            if (SM[sockfd].recv_buf.recv_buffer[i].recvd==0 && SM[sockfd].recv_buf.recv_buffer[i].occupied==1){
                // means a valid msg that can be received
                //printf("c: %s\n", SM[sockfd].recv_buf.recv_buffer[i].msg);
                strcpy(buf, SM[sockfd].recv_buf.recv_buffer[i].msg);
                //printf("recvd seq:%d\n", SM[sockfd].recv_buf.recv_buffer[i].seq);
                SM[sockfd].recv_buf.recv_buffer[i].recvd=0;
                SM[sockfd].recv_buf.recv_buffer[i].occupied=0;
                memset(SM[sockfd].recv_buf.recv_buffer[i].msg, 0, MSG_SIZE);
                SM[sockfd].recv_window.recv_wnd_size--;
                SM[sockfd].recv_window.start_index++;
                SM[sockfd].recv_window.start_index = SM[sockfd].recv_window.start_index % RECV_BUF_SIZE;
                return_value = min(len, MSG_SIZE);
                printf("recv: s:%d l:%d\n", SM[sockfd].recv_window.start_index, SM[sockfd].recv_window.recv_wnd_size);
                //break;
            }
            //printf("here\n");
            // i++;
            // i = i%RECV_BUF_SIZE;
        //}
        if (i == index){
            // no msg came
            return_value = -1;
            errno = ENOMSG;
        }
    }
    else {
        // not bound
        return_value = -1;
        errno = ENOTCONN;
    }
    
    mtx_op.sem_op = 1;
    semop(mutex, &mtx_op, 1);
    shmdt(SM);
    return return_value;
}
int m_close(int sockfd){
    int shm_SM = shmget(3501, sizeof(mtp_socket_info)*MAX_SOCKETS, 0777|IPC_CREAT);
    mtp_socket_info*SM = (mtp_socket_info*) shmat(shm_SM, 0, 0);
    int mutex;
    mutex = semget(5000, 1, 0777|IPC_CREAT);
    struct sembuf mtx_op;
    mtx_op.sem_flg = 0;
    mtx_op.sem_num = 0;

    int return_value = -1;

    mtx_op.sem_op = -1;
    semop(mutex, &mtx_op, 1);
    
    if (sockfd >= MAX_SOCKETS || sockfd < 0) {
        // Invalid socket descriptor
        printf("m_close: Invalid socket descriptor\n");
        errno = EBADF;
        return_value = -1;
    } else if (SM[sockfd].alloted == 0) {
        // Socket not allocated
        printf("m_close: Socket not allocated\n");
        errno = EINVAL;
        return_value = -1;
    } else {
        //close(SM[sockfd].udp_sock_id);

        //SM[sockfd].alloted = 0;

        return_value = 0;
    }
    
    mtx_op.sem_op = 1;
    semop(mutex, &mtx_op, 1);
    shmdt(SM);
    return return_value;
}

int dropMessage(float a) {
    float random_num = ((float) rand()) / RAND_MAX; // Generate random number between 0 and 1
    return (random_num < a) ? 1 : 0; // Return 1 if random_num is less than p, else return 0
}


void set_curr_time(struct timeval* tv){
    gettimeofday(tv, NULL);
}

int min(int x, int y){
    if (x<y) return x;
    return y;
}
int max(int x, int y){
    if (x<y) return y;
    return x;
}

void convertSeqToStr(int n, char* res){
    n = n%16;
    int j=3;
    //printf("n:%d\n", n);
    while(n>=0 && j>=0){
        if (n & 1){
            res[j]='1';
        }
        else res[j]='0';
        n = n>>1;
        j--;
    }
    //printf("num:%d-res:%c%c%c%c$\n", n, res[0], res[1], res[2], res[3]);
    return;
}


int convertStrToSeq(char* seq){
    int n=0;
    if (seq[0] == '1') n += 8;
    if (seq[1] == '1') n += 4;
    if (seq[2] == '1') n += 2;
    if (seq[3] == '1') n += 1;
    return n;
}

void semaphore_wait(int sem){
    struct sembuf sem_op;
    sem_op.sem_flg = 0;
    sem_op.sem_num = 0;
    sem_op.sem_op = -1;
    semop(sem, &sem_op, 1);
}
void semaphore_signal(int sem){
    struct sembuf sem_op;
    sem_op.sem_flg = 0;
    sem_op.sem_num = 0;
    sem_op.sem_op = 1;
    semop(sem, &sem_op, 1);
}