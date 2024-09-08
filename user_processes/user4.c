#include "msocket.h"

int main(){
    
    int mtp_sock = m_socket(AF_INET, SOCK_MTP, 0);
    printf("socket created\n");
    printf("mtp sock id: %d\n", mtp_sock);
    struct sockaddr_in serv_addr, cli_addr;
   memset(&serv_addr, 0, sizeof(serv_addr));
   memset(&cli_addr, 0, sizeof(cli_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_port = htons(8501);
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   cli_addr.sin_family = AF_INET;
   cli_addr.sin_port = htons(8500);
   cli_addr.sin_addr.s_addr = INADDR_ANY;
    char buf[MSG_SIZE];
    m_bind(mtp_sock, (const struct sockaddr*)&serv_addr, sizeof(serv_addr), (struct sockaddr*)&cli_addr, sizeof(cli_addr));
    printf("socket bound to port %d\n", serv_addr.sin_port);
    
    while(1){
        memset(buf, 0, MSG_SIZE);
        sleep(3);
        m_recvfrom(mtp_sock, buf, MSG_SIZE, 0, (struct sockaddr*)&cli_addr, NULL);
        printf("[+] %s\n", buf);
    }

    sleep(50);
    printf("closing\n");
    m_close(mtp_sock);
    
    return 0;
}