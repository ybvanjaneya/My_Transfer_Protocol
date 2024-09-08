#include "msocket.h"
#include <fcntl.h>


int main(){
    int mtp_sock = m_socket(AF_INET, SOCK_MTP, 0);
	printf("socket created\n");    
    printf("mtp Socket id: %d\n", mtp_sock);
   struct sockaddr_in serv_addr, cli_addr;
   memset(&serv_addr, 0, sizeof(serv_addr));
   memset(&cli_addr, 0, sizeof(cli_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_port = htons(9501);
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   cli_addr.sin_family = AF_INET;
   cli_addr.sin_port = htons(9500);
   cli_addr.sin_addr.s_addr = INADDR_ANY;
   char buf[MSG_SIZE];
    m_bind(mtp_sock, (const struct sockaddr*)&serv_addr, sizeof(serv_addr), (struct sockaddr*)&cli_addr, sizeof(cli_addr));
    printf("socket bound to port %d\n", serv_addr.sin_port);
    printf("receiving file from user_1\n");
    int fd = open("test1.rec.txt", O_CREAT|O_WRONLY|O_TRUNC, 0777);
    int flag = 0;
    sleep(1);
    while(1){
        memset(buf, 0, MSG_SIZE);
        int n = m_recvfrom(mtp_sock, buf, MSG_SIZE, 0, (struct sockaddr*)&cli_addr, NULL);
        //if (n<0) break;
        if(buf[0]!='\0')printf("[+] %s\n", buf);
        for(int i=0; i<n; i++){
            if (buf[i] == '$'){
                flag = 1;
                break;
            }
            write(fd, &buf[i], 1);
        }
        if (flag ==1 ) break;
        usleep(5000);
    }
    close(fd);
    sleep(3);
    m_close(mtp_sock);
   printf("closing\n");
   
    return 0;
}
