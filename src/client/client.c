#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


static int connect_to(const char* ip,int port){
    int fd=socket(AF_INET,SOCK_STREAM,0); if(fd<0){perror("socket");exit(1);}
    struct sockaddr_in s={0}; s.sin_family=AF_INET; s.sin_port=htons(port);
    if(inet_pton(AF_INET,ip,&s.sin_addr)!=1){perror("inet_pton");exit(1);}
    if(connect(fd,(struct sockaddr*)&s,sizeof(s))<0){perror("connect");exit(1);}
    return fd;
}
static int recv_line(int fd,char*buf,size_t cap){
    size_t n=0;char ch;while(n+1<cap){ssize_t r=recv(fd,&ch,1,0);if(r<=0)return -1;if(ch=='\n'){buf[n]=0;return n;}buf[n++]=ch;}buf[n]=0;return n;}
static void render_state(const int b[12],int s0,int s1,int cur){
    printf("\n    P2 (%d pts)\n",s1);
    printf("-------------------------\n");
    for(int i=11;i>=6;i--)printf("%2d  ",i);printf("\n");
    printf("+---+---+---+---+---+---+\n|");
    for(int i=11;i>=6;i--)printf("%2d |",b[i]);printf("\n+---+---+---+---+---+---+\n|");
    for(int i=0;i<=5;i++)printf("%2d |",b[i]);printf("\n+---+---+---+---+---+---+\n ");
    for(int i=0;i<=5;i++)printf("%2d  ",i);printf("\n--------------------------\n");
    printf("    P1 (%d pts)   (au tour de P%d)\n\n",s0,cur+1);
}
int main(int argc,char**argv){
    if(argc<3){fprintf(stderr,"Usage: %s <ip> <port>\n",argv[0]);return 1;}
    int fd=connect_to(argv[1],atoi(argv[2]));char buf[256];int myrole=-1;
    while(1){
        if(recv_line(fd,buf,sizeof(buf))<0){puts("Déconnecté.");break;}
        if(!strncmp(buf,"ROLE ",5)){myrole=atoi(buf+5);printf("Vous êtes P%d.\n",myrole+1);}
        else if(!strncmp(buf,"STATE ",6)){
            int b[12],s0,s1,cur;
            if(sscanf(buf+6,"%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
                &b[0],&b[1],&b[2],&b[3],&b[4],&b[5],
                &b[6],&b[7],&b[8],&b[9],&b[10],&b[11],
                &s0,&s1,&cur)==15){
                render_state(b,s0,s1,cur);
                if(myrole==cur){
                    printf("Votre tour. Entrez un pit (ou 'd' pour DRAW): ");
                    if(!fgets(buf,sizeof(buf),stdin))break;
                    if(buf[0]=='d'||buf[0]=='D')send(fd,"DRAW\n",5,0);
                    else{int pit=atoi(buf);char out[64];snprintf(out,sizeof(out),"MOVE %d\n",pit);send(fd,out,strlen(out),0);}
                }
            }
        } else if(!strncmp(buf,"ASKDRAW",7)){
            printf("L’adversaire propose l’égalité. Accepter ? (o/n): ");
            if(!fgets(buf,sizeof(buf),stdin))break;
            if(buf[0]=='o'||buf[0]=='O')send(fd,"YES\n",4,0);
            else send(fd,"NO\n",3,0);
        } else if(!strncmp(buf,"MSG ",4)){puts(buf+4);}
        else if(!strncmp(buf,"END ",4)){puts(buf);break;}
        else puts(buf);
    }
    close(fd);return 0;
}
