#include "msocket.h"

void* R(void* arg);
void* S(void * arg);
void* G(void* arg);
sock_info* SOCK_INFO;
mtp_socket_info* SM;
struct sembuf mtx_op;
int mutex;
void sigint_handler(){
    // cleaning the shared memory segments by the end of init process
    int shm_SOCK_INFO = shmget(3500, sizeof(sock_info), IPC_CREAT|0777);
    int shm_SM = shmget(3501, sizeof(mtp_socket_info)*MAX_SOCKETS, 0777|IPC_CREAT);
    shmctl(shm_SOCK_INFO, IPC_RMID, 0);
    shmctl(shm_SM, IPC_RMID, 0);
    // cleaning the semaphores created
    int sem1 = semget(4100, 1, 0777|IPC_CREAT);
    int sem2 = semget(4101, 1, 0777|IPC_CREAT);
    semctl(sem1, 0, IPC_RMID, 0);
    semctl(sem2, 0, IPC_RMID, 0);
    semctl(mutex, 0, IPC_RMID, 0);
    kill(getpid(), SIGKILL);
}
pthread_mutex_t thread_mutex= PTHREAD_MUTEX_INITIALIZER;

int main(){
    // creating the shared memory segments
    int shm_SOCK_INFO = shmget(3500, sizeof(sock_info), IPC_CREAT|0777);
    SOCK_INFO = (sock_info*)shmat(shm_SOCK_INFO, 0, 0 );
    SOCK_INFO->err=0;
    SOCK_INFO->port = 0;
    SOCK_INFO->sock_id = 0;
    memset(SOCK_INFO->ip, 0, INET_ADDRSTRLEN);
    signal(SIGINT, sigint_handler); 
    signal(SIGSEGV, sigint_handler);
    int shm_SM = shmget(3501, sizeof(mtp_socket_info)*MAX_SOCKETS, 0777|IPC_CREAT);
    SM = (mtp_socket_info*) shmat(shm_SM, 0, 0);

    for(int i=0; i<MAX_SOCKETS; i++){
        SM[i].alloted = 0;
        SM[i].send_buf.send_buffer[SEND_BUF_SIZE-1].seq = 0;
        SM[i].recv_buf.last_inorder_seq = 0;
        SM[i].recv_buf.flag_nospace = 0;
        SM[i].send_window.start_index = 0;
        SM[i].recv_window.start_index = 0;
        SM[i].send_window.send_wnd_size= 0;
        SM[i].recv_window.recv_wnd_size= 0;
        SM[i].send_buf.tot_msgs=0;
        SM[i].send_buf.tot_sends=0;
        SM[i].send_buf.recv_buf_size = RECV_BUF_SIZE;
        for(int j=0; j<RECV_BUF_SIZE; j++){
            SM[i].recv_window.sequence[j] = j%SEQ;
        }
    }

    // creating the semaphores
    int sem1, sem2;
    struct sembuf sem1_op;
    struct sembuf sem2_op;
    sem1_op.sem_flg = 0;
    sem1_op.sem_num = 0;
    sem2_op.sem_flg = 0;
    sem2_op.sem_num = 0;
    // mutex for sendto and recvfrom
    mutex = semget(5000, 1, 0777|IPC_CREAT);
    
    mtx_op.sem_flg = 0;
    mtx_op.sem_num = 0;
    semctl(mutex, 0, SETVAL, 1);

    sem1 = semget(4100, 1, 0777|IPC_CREAT);
    sem2 = semget(4101, 1, 0777|IPC_CREAT);

    semctl(sem1, 0, SETVAL, 0);
    semctl(sem1, 0, SETVAL, 0);

    // creating the threads R and S
    pthread_t R_thread, S_thread, G_thread;
    pthread_create(&R_thread, NULL, R, NULL);
    pthread_create(&S_thread, NULL, S, NULL);
    pthread_create(&G_thread, NULL, G, NULL);

    while(1){
        // wait on sem1
        //printf("waiting\n");
        sem1_op.sem_op = -1;
        semop(sem1, &sem1_op, 1);
        //semaphore_wait(mutex);

        if (SOCK_INFO->err==0 && SOCK_INFO->port == 0 && SOCK_INFO->sock_id==0){
            // m_socket call
            int udp_socket=-1;
            udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
            printf("udp: %d\n", udp_socket);
            if (udp_socket == -1){
                SOCK_INFO->sock_id = -1;
                SOCK_INFO->err = errno;
            }
            else{
                printf("socket created %d\n", udp_socket);
                SOCK_INFO->sock_id = udp_socket;
            }
        }
        else if (SOCK_INFO->sock_id != 0 && SOCK_INFO->port != 0){
            // m_bind call
            struct sockaddr_in servaddr;
            memset(&servaddr, 0, sizeof(servaddr));
            servaddr.sin_family = AF_INET;
            servaddr.sin_addr.s_addr = inet_addr(SOCK_INFO->ip);
            servaddr.sin_port = htons(SOCK_INFO->port);
            int bind_return = bind(SOCK_INFO->sock_id, (const struct sockaddr*)&servaddr, sizeof(servaddr));
            if (bind_return == -1){
                SOCK_INFO->sock_id = -1;
                SOCK_INFO->err = errno;
            }
        }
        //semaphore_signal(mutex);
        // signal on sem2
        sem2_op.sem_op = 1;
        semop(sem2, &sem2_op, 1);
    }
    

    // waiting for the threads to end
    pthread_join(R_thread, NULL);
    pthread_join(S_thread, NULL);
    pthread_join(G_thread, NULL);
    // cleaning the shared memory segments by the end of init process
    shmdt(SOCK_INFO);
    shmdt(SM);
    shmctl(shm_SOCK_INFO, IPC_RMID, 0);
    shmctl(shm_SM, IPC_RMID, 0);
    // cleaning the semaphores created
    semctl(sem1, 0, IPC_RMID, 0);
    semctl(sem2, 0, IPC_RMID, 0);
    semctl(mutex, 0, IPC_RMID, 0);
    return 0;
}

void* R(void* arg){
    fd_set readfds;
    int nospace = 0;
    while(1){
        FD_ZERO(&readfds);
        int maxfd=0;
        struct timeval tv;
        tv.tv_sec = T;
        tv.tv_usec = 0;
        // mtx_op.sem_op = -1;
        // semop(mutex, &mtx_op, 1);   // wait
        pthread_mutex_lock(&thread_mutex);
        for(int i=0; i<MAX_SOCKETS; i++){
            if (SM[i].alloted == 1){
                if (kill(SM[i].pid, 0)!=0){
                    // killed process
                    
                }
                else {
                    FD_SET(SM[i].udp_sock_id, &readfds);
                    maxfd = max(maxfd, SM[i].udp_sock_id);
                }
            }
        }
        pthread_mutex_unlock(&thread_mutex);
        int activity = select(maxfd+1, &readfds, NULL, NULL,&tv );
        if (activity == -1){
            perror("Select");
        }
        else if (activity == 0){
            // time  out happend
            pthread_mutex_lock(&thread_mutex);
            for (int i=0; i<MAX_SOCKETS; i++){
                if (SM[i].alloted==1){
                    if (kill(SM[i].pid, 0)!=0){} // killed process
                    else {
                        if (SM[i].recv_buf.flag_nospace==1){
                            int sp = 0;
                            for(int j=0; j<RECV_BUF_SIZE; j++){
                                if (SM[i].recv_buf.recv_buffer[j].occupied==0){
                                    sp++;
                                }
                            }
                            if (sp > 0){
                                // now space is available
                                SM[i].recv_buf.flag_nospace = 0;
                                char frame[FRAME_SIZE];
                                memset(frame, 0, FRAME_SIZE);
                                char seq[4];
                                convertSeqToStr(SM[i].recv_buf.last_inorder_seq, seq);
                                //sprintf(frame, "ACK-%s-%d", seq, sp);
                                strcpy(frame, "ACK-");
                                sprintf(&frame[4], "%s", seq);
                                strcpy(&frame[8], "-");
                                sprintf(&frame[9],"%d", sp);
                                int by_sent = sendto(SM[i].udp_sock_id, frame, strlen(frame), 0, (const struct sockaddr*)&SM[i].other, sizeof(SM[i].other));
                                if (by_sent < 0){
                                    printf("ack-sent error\n");
                                }
                            }
                        }
                    }
                }
            }
        }
        else {
            // check for every socket
            pthread_mutex_lock(&thread_mutex);
            for (int i=0; i<MAX_SOCKETS; i++){
                if (SM[i].alloted==1){
                    if (kill(SM[i].pid, 0)!=0){} // killed process
                    else {
                        if (FD_ISSET(SM[i].udp_sock_id, &readfds)){
                            char frame[FRAME_SIZE];
                            memset(frame, 0, FRAME_SIZE);
                            int by_recv = recvfrom(SM[i].udp_sock_id, frame, FRAME_SIZE, 0, (struct sockaddr*)&SM[i].other, NULL);
                            printf("recvd\n");
                            frame[FRAME_SIZE-1]='\0';
                            frame[FRAME_SIZE-2]='\0';
                            frame[FRAME_SIZE-3]='\0';
                            printf("frame:%s\n", frame);
                            printf("sock: %d pid: %d\n",i, SM[i].pid);
                            int k = dropMessage(p);
                            if (k==1){
                                printf("message droppped\n");
                                continue;
                            }
                            if (strncmp(frame, "DAT", 3)==0){
                                // data frame
                                char seq[4];
                                char msg[MSG_SIZE];
                                sscanf(frame, "DAT-%s-%s", seq, msg);
                                strcpy(msg, &frame[9]);
                                int n = convertStrToSeq(seq);
                                int expected = (SM[i].recv_buf.last_inorder_seq+1)%SEQ;
                                int index = (SM[i].recv_window.start_index+SM[i].recv_window.recv_wnd_size)%RECV_BUF_SIZE;
                                int sp=0;
                                if (expected == n){
                                    // inorder msg came
                                    if (SM[i].recv_buf.recv_buffer[index].occupied==1){
                                        // no buffers
                                        printf("No buffer to take the msg\n");
                                        sp = 0;
                                    }
                                    else {
                                        SM[i].recv_window.recv_wnd_size++;
                                        SM[i].recv_buf.last_inorder_seq = expected;
                                        set_curr_time(&SM[i].recv_buf.recv_buffer[index].recv_time);
                                        SM[i].recv_buf.recv_buffer[index].occupied = 1;
                                        SM[i].recv_buf.recv_buffer[index].recvd = 0;
                                        SM[i].recv_buf.recv_buffer[index].seq = n;
                                        //printf("MSG: %s\n", msg);
                                        sp = RECV_BUF_SIZE-SM[i].recv_window.recv_wnd_size;
                                        strcpy(SM[i].recv_buf.recv_buffer[index].msg, msg);
                                        printf("s:%d l:%d\n", SM[i].recv_window.start_index, SM[i].recv_window.recv_wnd_size);

                                    }
                                }
                                if (sp == 0){
                                    SM[i].recv_buf.flag_nospace = 1;
                                }
                                memset(frame, 0, FRAME_SIZE);
                                
                                convertSeqToStr(SM[i].recv_buf.last_inorder_seq, seq);
                                // sprintf(frame, "ACK-%s-%d", seq, sp);
                                strcpy(frame, "ACK-");
                                sprintf(&frame[4], "%s", seq);
                                strcpy(&frame[8], "-");
                                sprintf(&frame[9],"%d", sp);
                                int by_sent = sendto(SM[i].udp_sock_id, frame, strlen(frame), 0, (const struct sockaddr*)&SM[i].other, sizeof(SM[i].other));
                                if (by_sent < 0){
                                    printf("ack-sent error\n");
                                }
                            }
                            else {
                                // ack frame
                                char seq[4];
                                int rwnd;
                                sscanf(frame, "ACK-%s-%d", seq, &rwnd);
                                rwnd = atoi(&frame[9]);
                                printf("recvd rwnd size:%d\n", rwnd);
                                int n = convertStrToSeq(seq);
                                if (SM[i].send_buf.last_ack_seq!=n && SM[i].send_window.send_wnd_size!=0){
                                    // not a duplicate ack
                                    int index = (SM[i].send_window.start_index+SM[i].send_window.send_wnd_size )%SEND_BUF_SIZE;
                                    int j= SM[i].send_window.start_index;
                                    int found=0;
                                    int k = SM[i].send_window.start_index;
                                    int count = 0;
                                    while(1){
                                        if (index>=SM[i].send_window.start_index){
                                            if (j<SM[i].send_window.start_index || j>=index){break;}
                                            
                                        }
                                        if (index<SM[i].send_window.start_index){
                                            if (j<SM[i].send_window.start_index && j>=index){break;}
                                            
                                        }
                                        if (SM[i].send_buf.send_buffer[j].seq==n){
                                            found =1;
                                            k = j;
                                            break;
                                        }
                                        j++;
                                        count++;
                                        j = j%SEND_BUF_SIZE;
                                    }
                                    if (found == 1) {
                                        j = SM[i].send_window.start_index;
                                        while(1){
                                            if (index>=SM[i].send_window.start_index){
                                                if (j<SM[i].send_window.start_index || j>=index){break;}
                                                
                                            }
                                            if (index<SM[i].send_window.start_index){
                                                if (j<SM[i].send_window.start_index && j>=index){break;}
                                                
                                            }
                                            SM[i].send_buf.send_buffer[j].occupied=0;
                                            SM[i].send_buf.send_buffer[j].sent = 0;
                                            j++;
                                            j = j%SEND_BUF_SIZE;
                                        }
                                        SM[i].send_window.send_wnd_size-=(count+1);
                                        SM[i].send_window.start_index = (k+1)%SEND_BUF_SIZE;
                                    }
                                }
                    
                                SM[i].send_buf.recv_buf_size = rwnd;
                            }
                        }
                    }
                }
            }
        }
        pthread_mutex_unlock(&thread_mutex);
        // mtx_op.sem_op = 1;
        // semop(mutex, &mtx_op, 1);   // signal
    }
    return NULL;
}
void* S(void* arg){
    while(1){
        //sleep(T/2-1);
        sleep(2);
        usleep(5);
        // mtx_op.sem_op = -1;
        // semop(mutex, &mtx_op, 1);   // wait
        printf("s thread woke up\n");
        pthread_mutex_lock(&thread_mutex);
        printf("S thread checking timeout\n");
        for (int i=0; i<MAX_SOCKETS; i++){
            if (SM[i].alloted == 1){
                // if (kill(SM[i].pid, 0)!=0){
                //     // killed process
                    
                // }
                // else {
                    printf("id:%d \n", i);
                    int index = (SM[i].send_window.start_index + SM[i].send_window.send_wnd_size)%SEND_BUF_SIZE;
                    struct timeval current_time;
                    set_curr_time(&current_time);
                    long time_diff = (current_time.tv_sec - SM[i].send_buf.send_buffer[SM[i].send_window.start_index].sent_time.tv_sec) * 1000000L +
                                        current_time.tv_usec - SM[i].send_buf.send_buffer[SM[i].send_window.start_index].sent_time.tv_usec;

                    if (time_diff >= T*1000000L){
                        // timeout occured retransmit all themessages in the window
                        int j= SM[i].send_window.start_index;
                        while(1){
                            if ( index>=SM[i].send_window.start_index ){
                                if (j<SM[i].send_window.start_index || j>=index){
                                    break;
                                }
                            }
                            if ( index<=SM[i].send_window.start_index){
                                if (j<SM[i].send_window.start_index && j>=index){
                                    break;
                                }
                            }
                            char frame[FRAME_SIZE];
                            memset(frame, 0, FRAME_SIZE);
                            // DATA frame
                            char seq[4]={'0'};
                            convertSeqToStr(SM[i].send_buf.send_buffer[j].seq, seq);
                            //sprintf(frame, "DAT-%s-%s", seq, SM[i].send_buf.send_buffer[j].msg);
                            strcpy(frame, "DAT-");
                            sprintf(&frame[4], "%s", seq);
                            strcpy(&frame[8], "-");
                            strcpy(&frame[9], SM[i].send_buf.send_buffer[j].msg);
                            printf("sent:%s\n", frame);
                            int by_sent = sendto(SM[i].udp_sock_id, frame, FRAME_SIZE, 0, (const struct sockaddr*)&SM[i].other, sizeof(SM[i].other));
                            if (by_sent < 0){
                                printf("timeout-sent error\n");
                            }
                            SM[i].send_buf.tot_sends++;
                            SM[i].send_buf.send_buffer[j].sent = 1;
                            set_curr_time(&SM[i].send_buf.send_buffer[j].sent_time );
                            j++;
                            j = j%SEND_BUF_SIZE;
                        }
                    }
                    // send any messages which are ready to send
                    int j=index;
                    if (SM[i].send_buf.recv_buf_size <= 0){
                        continue;
                    }
                    printf("entering send loop\n");
                    int k = SM[i].send_buf.recv_buf_size;
                    while(1){
                        //index = (SM[i].send_window.start_index + SM[i].send_window.send_wnd_size)%SEND_BUF_SIZE;
                        //j= index;
                        if (index>=SM[i].send_window.start_index){
                            if (j<SM[i].send_window.start_index || j>=index){}
                            else break;
                        }
                        if (index<SM[i].send_window.start_index){
                            if (j<SM[i].send_window.start_index && j>=index){}
                            else break;
                        }
                        
                        
                        if (SM[i].send_buf.send_buffer[j].occupied==1 && SM[i].send_buf.send_buffer[j].sent==0){
                            // not yet sent 
                            //printf("msg: %d sent\n", j);
                            if (k>0){
                                char frame[FRAME_SIZE];
                                memset(frame, 0, FRAME_SIZE);
                                // DATA frame
                                char seq[4]={'0'};
                                convertSeqToStr(SM[i].send_buf.send_buffer[j].seq, seq);
                                //printf("sent over udp:%s\n", SM[i].send_buf.send_buffer[j].msg);
                                //printf("msg:%s\n", SM[i].send_buf.send_buffer[j].msg);
                                //sprintf(frame, "DAT-%s-%s", seq, SM[i].send_buf.send_buffer[j].msg);
                                strcpy(frame, "DAT-");
                                sprintf(&frame[4], "%s", seq);
                                strcpy(&frame[8], "-");
                                strcpy(&frame[9], SM[i].send_buf.send_buffer[j].msg);
                                printf("sent:%s\n", frame);
                                int by_sent = sendto(SM[i].udp_sock_id, frame, FRAME_SIZE, 0, (const struct sockaddr*)&SM[i].other, sizeof(SM[i].other));
                                if (by_sent < 0){
                                    printf("data-sent error\n");
                                    break;
                                }
                                SM[i].send_buf.tot_sends++;
                                SM[i].send_window.send_wnd_size++;
                                SM[i].send_buf.send_buffer[j].sent = 1;
                                set_curr_time(&SM[i].send_buf.send_buffer[j].sent_time );
                                k--;
                            }
                            else break;
                        }
                        else {
                            break;
                        }
                        j++;
                        j = j%SEND_BUF_SIZE;
                    }
                //}
            }
        }
        pthread_mutex_unlock(&thread_mutex);
        // mtx_op.sem_op = 1;
        // semop(mutex, &mtx_op, 1);   // signal
    }
    
    return NULL;    
}
void* G(void* arg){
    while(1){
        sleep(9);
        // mtx_op.sem_op = -1;
        // semop(mutex, &mtx_op, 1);  // wait
        pthread_mutex_lock(&thread_mutex);
        for(int i=0; i<MAX_SOCKETS; i++){
            // close those udp sockets for killed processes
            if (SM[i].alloted==1 && kill(SM[i].pid, 0)!=0 ) {
                printf("** FOR: mtp_sock:%d - pid:%d\n", i, SM[i].pid);
                printf("TOTAL MSGS:%d\nTOTAL SENDS:%d  **\n", SM[i].send_buf.tot_msgs, SM[i].send_buf.tot_sends);
                close(SM[i].udp_sock_id);
                SM[i].alloted = 0;
                SM[i].send_buf.send_buffer[SEND_BUF_SIZE-1].seq = 0;
                SM[i].recv_buf.last_inorder_seq = 0;
                SM[i].recv_buf.flag_nospace = 0;
                SM[i].send_window.start_index = 0;
                SM[i].recv_window.start_index = 0;
                SM[i].send_window.send_wnd_size= 0;
                SM[i].recv_window.recv_wnd_size= 0;
                SM[i].send_buf.tot_msgs=0;
                SM[i].send_buf.tot_sends=0;
                SM[i].send_buf.recv_buf_size = RECV_BUF_SIZE;
                for(int j=0; j<RECV_BUF_SIZE; j++){
                    SM[i].recv_window.sequence[j] = j%SEQ;
                }
            }
        }
        pthread_mutex_unlock(&thread_mutex);
        // mtx_op.sem_op = 1;
        // semop(mutex, &mtx_op, 1);   // signal
    }
    return NULL;
}