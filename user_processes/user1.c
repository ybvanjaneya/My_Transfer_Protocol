#include "msocket.h"
#include <fcntl.h>

int main(){
    int mtp_sock = m_socket(AF_INET, SOCK_MTP, 0);
    printf("socket created\n");
    printf("mtp sock id: %d\n", mtp_sock);
    struct sockaddr_in serv_addr, cli_addr;
   memset(&serv_addr, 0, sizeof(serv_addr));
   memset(&cli_addr, 0, sizeof(cli_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_port = htons(9500);
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   cli_addr.sin_family = AF_INET;
   cli_addr.sin_port = htons(9501);
   cli_addr.sin_addr.s_addr = INADDR_ANY;
    char buf[MSG_SIZE];
    m_bind(mtp_sock, (const struct sockaddr*)&serv_addr, sizeof(serv_addr), (struct sockaddr*)&cli_addr, sizeof(cli_addr));
    printf("socket bound to port %d\n", serv_addr.sin_port);
    sleep(3);
    // sending file
    int fd = open("test1.txt", O_RDONLY, 0777);
    if (fd == -1){
        printf("no file named test1.txt found\n");
    }
    while(1){
        memset(buf, 0, MSG_SIZE);
        if (read(fd, buf, MSG_SIZE)<=0) break;
        m_sendto(mtp_sock, buf, MSG_SIZE, 0, (const struct sockaddr*)&cli_addr, sizeof(cli_addr));
        sleep(4);
    }
    sleep(3);
    memset(buf, 0, MSG_SIZE);
    sprintf(buf, "$");
    m_sendto(mtp_sock, buf, MSG_SIZE, 0, (const struct sockaddr*)&cli_addr, sizeof(cli_addr));
    close(fd);
    sleep(20);
    printf("closing\n");
    m_close(mtp_sock);
    return 0;
}