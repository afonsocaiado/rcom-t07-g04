/*      (C)2000 FEUP  */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <strings.h>

#include "getip.c"

#define SERVER_PORT 21 // porta do FTP

// struct que vai guardar toda a informação contida no url 
struct urlInfo
{
   char protocol[6]; // string que irá conter o protocolo
   char username[256]; // string que irá conter o username
   char password[256]; // string que irá conter a password
   char host[256]; // string que irá conter o host
   char path[1024]; // string que irá conter o path para o respetivo ficheiro
   char filename[512]; // string que irá conter o nome do ficheiro
};


int downloadFileFromSever(struct urlInfo url){

	int	sockfd;
	struct	sockaddr_in server_addr;
	char *ip = getIp(url.host);  
	int	bytes;
	
	printf("Ip: %s\n",ip);
	
	/*server address handling*/
	bzero((char*)&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip);	/*32 bit Internet address network byte ordered*/
	server_addr.sin_port = htons(SERVER_PORT);		/*server TCP port must be network byte ordered */
    
	/*open an TCP socket*/
	if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
    		perror("socket()");
        	exit(0);
    	}
		
	/*connect to the server*/
    	if(connect(sockfd, 
	           (struct sockaddr *)&server_addr, 
		   sizeof(server_addr)) < 0){
        	perror("connect()");
		exit(0);
	}

    	/*send a string to the server*/
	//bytes = write(sockfd, buf, strlen(buf));
	//printf("Bytes escritos %d\n", bytes);

	close(sockfd);
	exit(0);
}



