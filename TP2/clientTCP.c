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
 * faz download do ficheiro especificado pelo utilizador 
 * @param url struct que contém toda a informação necessária para o download do ficheiro
 * @return 0 em caso de sucesso outro em caso de erro
 */ 
int downloadFileFromSever(struct urlInfo url){

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

		if (reachedTheEnd(buf)){ // se chegou ao fim da mensagem 
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
		while (strncmp(buf,"331 ",4) != 0) // limpar as msg enviadas pelo servidor
		{
			// RESPOSTA USER
			bzero(buf,sizeof(buf)); // limpar o buffer
			read(sockfdA, buf, 1024); // ler a resposta 
			printf("%s",buf);
		}
		
		// se o utilizador não colocou a password no url
		if ( strlen(url.password) == 0){
			getPassword(&url);
		}

		// ENVIAR PASS
		char password[7+strlen(url.password)]; // string que vai guardar o comando para introduzir a pass
		sprintf(password,"pass %s\r\n",url.password); // contrução do comando pass
		write(sockfdA, password, sizeof(password)); // enviar o comando pass <password>
		printf("%s",password);

		// REPOSTA PASS	
		bzero(buf,sizeof(buf)); // limpar o buffer
		read(sockfdA, buf, 1024); // ler a resposta ao comando pass
		printf("%s",buf);

		// verificar a resposta enviada pelo servidor 
		if (strncmp(buf,"230",3) != 0){ // se a resposta não for a desejada
			close(sockfdA);
			exit(-10);
		}
		while (strncmp(buf,"230 ",4) != 0) // limpar as msg enviadas pelo servidor
		{
			// RESPOSTA PASS
			bzero(buf,sizeof(buf)); // limpar o buffer
			read(sockfdA, buf, 1024); // ler a resposta 
			printf("%s",buf);
		}
	}
	
	// ENVIAR PASV
	char pasv[7]; // string que vai guardar o comando de pasv
	sprintf(pasv,"pasv\r\n"); // construção do comando pasv
	write(sockfdA, pasv, strlen(pasv)); // enviar o comando pasv
	printf("%s",pasv);

	// RESPOSTA PASV
	bzero(buf,sizeof(buf)); // limpar o buffer
	read(sockfdA, buf, 1024); // ler a resposta ao comando pasv
	printf("%s",buf);

	// verificar a resposta enviada pelo servidor 
	if (strncmp(buf,"227",3) != 0){ // se a resposta não for a desejada
		close(sockfdA);
		exit(-11);
	}
	while (strncmp(buf,"227 ",4) != 0) // limpar as msg enviadas pelo servidor
	{
		// RESPOSTA PASV
		bzero(buf,sizeof(buf)); // limpar o buffer
		read(sockfdA, buf, 1024); // ler a resposta 
		printf("%s",buf);
	}
	
	int porta = getPort(buf);
	
	// ENVIAR TYPE I - ALTERAR PARA BINARY MODE
	char type[]= "type I\r\n"; // string que vai guardar o comando type I
	write(sockfdA, type, strlen(type));  // enviar comando type I
	printf("%s",type);

	// RESPOSTA TYPE I
	bzero(buf,sizeof(buf)); 
	read(sockfdA, buf, 1024); 
	printf("%s",buf);


	// verificar a resposta enviada pelo servidor 
	if (strncmp(buf,"200",3) != 0){ // se a resposta não for a desejada
		close(sockfdA);
		exit(-12);
	}

	while (strncmp(buf,"200 ",4) != 0) // limpar as msg enviadas pelo servidor
	{
		// RESPOSTA TYPE
		bzero(buf,sizeof(buf)); // limpar o buffer
		read(sockfdA, buf, 1024); // ler a resposta 
		printf("%s",buf);
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

	if (strlen(url.path) != 0){
		// ENVIAR CWD
		char cwd[1024+6]; //string que vai conter o comando cwd juntamente como path do ficheiro
		sprintf(cwd,"cwd %s\r\n",url.path); // contrução do comando cwd
		write(sockfdA, cwd, strlen(cwd)); // enviar o comando cwd
		printf("%s",cwd);

		// RESPOSTA CWD
		bzero(buf,sizeof(buf)); // limpar o buffer
		read(sockfdA, buf, 1024); // ler a resposta ao comando cwd
		printf("%s",buf);

		// verificar a resposta enviada pelo servidor 
		if(strncmp(buf,"250",3) != 0){ // se a resposta não for a desejada
			close(sockfdA);
			close(sockfdB);
			exit(-15);
		}
		while (strncmp(buf,"250 ",4) != 0) // limpar as msg enviadas pelo servidor
		{
			// RESPOSTA CWD
			bzero(buf,sizeof(buf)); // limpar o buffer
			read(sockfdA, buf, 1024); // ler a resposta 
			printf("%s",buf);
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
	while (strncmp(buf,"150 ",4) != 0) // limpar as msg enviadas pelo servidor
	{
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

		if(bytes <= 0){ // quando o ficheiro chegar ao fim 
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



