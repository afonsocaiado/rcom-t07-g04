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

/**
 * função que verifica se chegou ao fim do cabeçalho enviado pelo servidor 
 * @param buf string enviada pelo servidor
 * @return 1 caso tenha chegado ao fim e 0 caso contrário
 */ 
int reachedTheEnd(char * buf){
	for (size_t i = 0; i < strlen(buf)-3; i++)
	{
		if ((atoi(&buf[i])== 220) && (buf[i+3] == ' ')) // verificar se na mensagem enviada encontra "220 " que indica o fim da mensagem 
			return 1;
	}
	return 0;
}


int downloadFileFromSever(struct urlInfo url){

	int	sockfd;
	struct	sockaddr_in server_addr;
	char *ip = getIp(url.host);  // obter o ip a partir do host
	char buf[1024]; 
	
	printf("Ip: %s\n",ip);
	
	/*server address handling*/
	bzero((char*)&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip);	/*32 bit Internet address network byte ordered*/
	server_addr.sin_port = htons(SERVER_PORT);		/*server TCP port must be network byte ordered */
    
	/*open an TCP socket*/
	if ((sockfd = socket(AF_INET,SOCK_STREAM ,0)) < 0) {
    		perror("socket()");
        	exit(-7);
    	}
		
	/*connect to the server*/
    	if(connect(sockfd, 
	           (struct sockaddr *)&server_addr, 
		   sizeof(server_addr)) < 0){
        	perror("connect()");
		exit(-8);
	}

	// ler tudo o que o servidor envia no inicio
	while (1)
	{
		bzero(buf,sizeof(buf)); // limpar o buffer
		read(sockfd, buf, 1024);
		printf("%s",buf);

		if (reachedTheEnd(buf)){ // se chegou ao fim da mensagem 
			break;
		}
	}

	// ENVIAR USER
    char username[7 + strlen(url.username)]; // string que vai guardar o comando para introduzir o username
	sprintf(username,"user %s\r\n",url.username); // contrução do comando user
	write(sockfd, username, sizeof(username)); // enviar o comando user <username>
	printf("%s",username);

	// RESPORTA USER
	bzero(buf,sizeof(buf)); // limpar o buffer
	read(sockfd, buf, 1024); // ler a resposta ao comando user
	printf("%s",buf);

	// verificar a resposta enviada pelo servidor 
	if (strncmp(buf,"331",3) != 0){ // se a resposta não for a desejada
		printf("Error: incorrect username\n");
		exit(-9);
	}

	// ENVIAR PASS
	char password[7+strlen(url.password)]; // string que vai guardar o comando para introduzir a pass
	sprintf(password,"pass %s\r\n",url.password); // contrução do comando pass
	write(sockfd, password, sizeof(username)); // enviar o comando pass <password>
	printf("%s",password);

	// REPOSTA PASS	
	bzero(buf,sizeof(buf)); // limpar o buffer
	read(sockfd, buf, 1024); // ler a resposta ao comando pass
	printf("%s",buf);

	// verificar a resposta enviada pelo servidor 
	if (strncmp(buf,"230",3) != 0){ // se a resposta não for a desejada
		printf("Error: incorrect password\n");
		exit(-10);
	}

	// ENVIAR PASV
	char pasv[7];
	sprintf(pasv,"PASV\r\n");
	write(sockfd, pasv, strlen(pasv)); // enviar o comando pasv
	printf("%s",pasv);

	// RESPOSTA PASV
	bzero(buf,sizeof(buf)); // limpar o buffer
	read(sockfd, buf, 1024); // ler a resposta ao comando pasv
	printf("%s",buf);

	close(sockfd);
	exit(0);
}



