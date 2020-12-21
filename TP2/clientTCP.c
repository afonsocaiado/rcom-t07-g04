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
	char username[256]; // string que irá conter o username
   	char password[256]; // string que irá conter a password
   	char host[256]; // string que irá conter o host
   	char path[1024]; // string que irá conter o path para o respetivo ficheiro
   	char filename[512]; // string que irá conter o nome do ficheiro
};

/**
 * função que verifica se chegou ao fim do cabeçalho enviado pelo servidor 
 * @param buf string enviada pelo servidor
 * @param number numero da resposta do servidor
 * @return 1 caso tenha chegado ao fim e 0 caso contrário
 */ 
int reachedTheEnd(char * buf, char *string){
	for (size_t i = 0; i < strlen(buf)-3; i++)
	{
		if (strncmp(&buf[i],string,4) == 0){ // verificar se na mensagem enviada encontra "string " que indica o fim da mensagem 
			return 1;
		}
	}
	return 0;
}

/**
 * a partir da resposta do servidor ao comando pasv retira e calcula a porta
 * @param buf string que contém a resposta do servidor ao comando pasv
 * @return porta de ligação 
 */ 
int getPort(char * buf){
	char * inicioDoIp = strchr(buf,'(');
	int portaParteA , portaParteB;

	// vai buscar a resposta do servidor apenas os ultimos dois numeros que servem para o calculo da porta
	sscanf(inicioDoIp,"(%*d,%*d,%*d,%*d,%d,%d",&portaParteA,&portaParteB);

	int porta = portaParteA*256 + portaParteB;

	return porta;
}

/**
 * pede ao utilizador para introduzir uma password 
 * @param url struct que contem a informação obtida do url
 */ 
void getPassword(struct urlInfo *url){
	printf("Password: ");
	char password[256]; // string que vai guardar a password inserida pelo utilizador
	scanf("%s",password); // lê do sdtin a password que o utilizador inseriu
	strncpy(url->password,password,strlen(password));
}

/**
 * função que envia comandos e recebe as respostas do servidor
 * @param sockfd identificador da ligação com o servidor 
 * @param preComando pré conteudo do comando que queremos enviar para o servidor
 * @param info informação que queremos enviar para o servidor
 * @param numero indica o numero da resposta que queremos obter do servidor
 * @return 1 em caso de sucesso 0 caso contrário
 */ 
int comunicationService(int sockfd,char *preComado,char *info,char *numero,char*buf,long buflength){

	// ENVIAR COMANDO
	char comando[strlen(preComado)+strlen(info)]; // string que vai guardar o comando juntamente com a informação (info)
	sprintf(comando,preComado,info); // contrução do comando 
	write(sockfd, comando, strlen(comando)); // enviar o comando para o servidor
	printf("%s",comando); // imprimir no ecrã o comando enviado para o servidor

	// RESPOSTA DO SERVIDOR
	bzero(buf,buflength); // limpar o buffer
	read(sockfd, buf, 1024); // ler a resposta dada pelo servidor
	printf("%s",buf); // imprimir no ecrã a resposta do servidor

	// verificar a resposta enviada pelo servidor 
	if (strncmp(buf,numero,3) != 0){ // se a resposta não for a desejada
		return 0;
	}
	while (1) // limpar as msg enviadas pelo servidor
	{
		if (reachedTheEnd(buf,numero)){ // se chegou ao fim da mensagem 
			break;
		}
		// RESPOSTA DO SERVIDOR
		bzero(buf,buflength); // limpar o buffer
		read(sockfd, buf, 1024); // ler a resposta
		printf("%s",buf);
	}
	return 1;
}


/**
 * faz download do ficheiro especificado pelo utilizador 
 * @param url struct que contém toda a informação necessária para o download do ficheiro
 * @return 0 em caso de sucesso outro em caso de erro
 */ 
int downloadFileFromServer(struct urlInfo url){

	int	sockfdA, sockfdB;
	struct	sockaddr_in server_addr;
	char *ip = getIp(url.host);  // obter o ip a partir do host
	char buf[1024]; 
	int bytes; // indica o numero de bytes lidos ou escritos
	
	printf("Ip: %s\n",ip);
	
	/*server address handling*/
	bzero((char*)&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip);	/*32 bit Internet address network byte ordered*/
	server_addr.sin_port = htons(SERVER_PORT);		/*server TCP port must be network byte ordered */
    
	/*open an TCP socket*/
	if ((sockfdA = socket(AF_INET,SOCK_STREAM ,0)) < 0) {
    		perror("socket()");
        	exit(-7);
    	}
		
	/*connect to the server*/
    	if(connect(sockfdA, 
	           (struct sockaddr *)&server_addr, 
		   sizeof(server_addr)) < 0){
        	perror("connect()");
		exit(-8);
	}

	// ler tudo o que o servidor envia no inicio
	while (1)
	{
		bzero(buf,sizeof(buf)); // limpar o buffer
		read(sockfdA, buf, 1024);
		printf("%s",buf);

		if (reachedTheEnd(buf,"220 ")){ // se chegou ao fim da mensagem 
			break;
		}
	}

	// ENVIAR USER
    char username[7 + strlen(url.username)]; // string que vai guardar o comando para introduzir o username
	sprintf(username,"user %s\r\n",url.username); // contrução do comando user
	write(sockfdA, username, sizeof(username)); // enviar o comando user <username>
	printf("%s",username);

	// RESPORTA USER
	bzero(buf,sizeof(buf)); // limpar o buffer
	read(sockfdA, buf, 1024); // ler a resposta ao comando user
	printf("%s",buf);

	// verificar a resposta enviada pelo servidor indica que não é necessário password
	if (strncmp(buf,"230",3) != 0){ 
		
		// verificar a resposta enviada pelo servidor 
		if (strncmp(buf,"331",3) != 0){ // se a resposta não for a desejada
			close(sockfdA);
			exit(-9);
		}
		while (1) // limpar as msg enviadas pelo servidor
		{
			if (reachedTheEnd(buf,"331 ")){ // se chegou ao fim da mensagem 
				break;
			}

			// RESPOSTA USER
			bzero(buf,sizeof(buf)); // limpar o buffer
			read(sockfdA, buf, 1024); // ler a resposta 
			printf("%s",buf);
			
		}
		
		// se o utilizador não colocou a password no url
		if ( strlen(url.password) == 0){
			getPassword(&url);
		}

		//envia o comando pass 
		if(!comunicationService(sockfdA,"pass %s\r\n",url.password,"230 ",buf,sizeof(buf))){
			close(sockfdA);
			exit(-10);
		}
	}
	
	//envia o comando pasv 
	if(!comunicationService(sockfdA,"pasv\r\n","","227 ",buf,sizeof(buf))){
		close(sockfdA);
		exit(-11);
	}
	
	int porta = getPort(buf);
	
	//envia o comando type I 
	if(!comunicationService(sockfdA,"type I\r\n","","200 ",buf,sizeof(buf))){
		close(sockfdA);
		exit(-12);
	}


	/*server address handling*/
	bzero((char*)&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip);	/*32 bit Internet address network byte ordered*/
	server_addr.sin_port = htons(porta);		/*server TCP port must be network byte ordered */
    
	/*open an TCP socket*/
	if ((sockfdB = socket(AF_INET,SOCK_STREAM ,0)) < 0) {
    		perror("socket()");
        	exit(-13);
    	}
		
	/*connect to the server*/
    	if(connect(sockfdB, 
	           (struct sockaddr *)&server_addr, 
		   sizeof(server_addr)) < 0){
        	perror("connect()");
		exit(-14);
	}

	if (strlen(url.path) != 0){ // ser for necessário alterar de directorio

		//envia o comando cwd
		if(!comunicationService(sockfdA,"cwd %s\r\n",url.path,"250 ",buf,sizeof(buf))){
			close(sockfdA);
			close(sockfdB);
			exit(-15);
		}
	
	}
	
	// ENVIAR RETR
	char retr[7+strlen(url.filename)]; // string que vai guardar o comando retr juntamente com o nome do ficheiro
	sprintf(retr,"retr %s\r\n",url.filename); // contrução do comando retr
	write(sockfdA, retr, strlen(retr)); // enviar o comando retr
	printf("%s",retr);

	// RESPOSTA RETR
	bzero(buf,sizeof(buf)); // limpar o buffer
	read(sockfdA, buf, 1024); // ler a resposta ao comando retr
	printf("%s",buf);


	// verificar a resposta enviada pelo servidor 
	if (strncmp(buf,"150",3) != 0){ // se a resposta não for a desejada
		close(sockfdA);
		close(sockfdB);
		exit(-16);
	}
	while (1) // limpar as msg enviadas pelo servidor
	{
		if (reachedTheEnd(buf,"150 ")){ // se chegou ao fim da mensagem 
			break;
		}
		// RESPOSTA RETR
		bzero(buf,sizeof(buf)); // limpar o buffer
		read(sockfdA, buf, 1024); // ler a resposta
		printf("%s",buf);
	}

	FILE * file;
	file = fopen(url.filename,"w"); // abrir o ficheiro para escrita

	while (1)
	{
		bzero(buf,sizeof(buf)); // limpar o buffer
		bytes = read(sockfdB, buf, 1024); // ler conteudo do ficheiro enviado

		if(bytes < 0){ // em caso de erro
			fclose(file);
			close(sockfdA);
			close(sockfdB);
			exit(0);
		}
		if(bytes == 0){ // quando o ficheiro chegar ao fim 
			break;
		}

		fwrite(buf,bytes,1,file); // escrever no ficheiro o conteudo lido 
	}

	// RESPOTA DE TRANFERENCIA COMPLETA
	bzero(buf,sizeof(buf)); // limpar o buffer
	read(sockfdA, buf, 1024); // ler a resposta do servidor após a transferência 
	printf("%s",buf);
	
	fclose(file);
	close(sockfdA);
	close(sockfdB);
	exit(0);
}



