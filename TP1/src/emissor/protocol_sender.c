#include <sys/types.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include "../protocol_utils.c"

#define DESLIGAR_ALARME 0
#define TEMPO_ESPERA 5
#define MAX_TENTATIVAS 3

enum A { AC = 0x03 , AR = 0x01 }; // valores possiveis do campo A na trama

speed_t BAUDRATE; // baudrate que irá ser utilizado na ligação série 

int Ns = 0; // numero de sequencia da trama do emissor
int Nr = 1; // numero de sequencia da trama do receptor

int time_out =FALSE; // flag que indica se ocorreu um time out
int num_tentativas = 0; // numero de tentativas de retransmissão

struct termios oldtio,newtio; //variaveis utilizadas para guardar a configuração da ligação pelo cabo serie

/**
 * função responsavel por lidar com os SIGALARM 
 **/
void atende()
{
  num_tentativas++;
  if (num_tentativas<=MAX_TENTATIVAS)
	  printf("Time Out...%i\n",num_tentativas);
  time_out = TRUE;
}

/**
 * altera os valores de Ns e Nr
 **/
void changeNSS(){
  if (Ns){
    Ns=0;
    Nr=1;
  }
  else{
    Ns=1;
    Nr=0;
  }
}

/**
* mecanismo de byte stuffing 
* @param buffer array de caracteres que vai sofrer byte stuffing
* @param length tamanho do array buffer
* @param new_buffer array que irá receber o conteudo do buffer após o byte stuffing
* @param BCC array que contem o(s) valore(s) do parametro BCC que protege os dados 
* @return tamanho do new_buffer 
**/
int byteStuffing(char * buffer,int length,char new_buffer[],char BCC[]){
  int j=0; // variavel que vai conter o tamanho de new_buffer, mas também irá funcionar de indice do array new_buffer

  //byte stuffing do buffer
  for (int i = 0; i < length; i++)
  {
    if (buffer[i] == FLAG){
      BCC[0] = BCC[0] ^ FLAG;
      new_buffer[j] = ESC;
      new_buffer[j+1] = ESCFLAG;
      j += 2;
    }else if (buffer[i] == ESC){
      BCC[0] = BCC[0]^ ESC;
      new_buffer[j] = ESC;
      new_buffer[j+1] = ESCESC;
      j += 2;
    }else{
      BCC[0] = BCC[0] ^ buffer[i];
      new_buffer[j] = buffer[i];
      j++;
    }
  }

  //verifica se o BCC é afetado pelo byte stuffing
  if(BCC[0] == FLAG){
    BCC[0] = ESC;
    BCC[1] = ESCFLAG;
  }else if (BCC[0] == ESC){
    BCC[0] = ESC;
    BCC[1] = ESCESC;
  }
  return j;
}

/**
 * envia a informação para o receptor (tramas de informação (I))
 * @param fd identificador da ligação de dados
 * @param trama array de caracteres a enviar 
 * @param trama_length indica o tamanho da trama a enviar 
 **/ 
void sendFrame_I(int fd, char * trama, int trama_length){

  int bytesSended = 0; // bytes que já foram enviados 
  int indice = 0; // indica o indice da trama onde deve começar a enviar a informação
  int remaning = trama_length; // indica a quantidade de bytes que faltam enviar 
  while (bytesSended != remaning )
  {
    bytesSended = write(fd,&trama[indice],remaning); // envia a informação para o receiver
    if ( (bytesSended != remaning) && (bytesSended != -1) ){
      indice += bytesSended; //atualiza o indice do array 
      remaning -=bytesSended; // decrementa a quantidade de bytes que faltam enviar
    }
  }
}

/**
 * cria um identificador de ligação de dados
 * @param  porta porta de comunicação
 * @return identificador de ligação em caso de sucesso e -1 em caso de erro
 **/ 
int llopen(int porta){
  int fd; // identificador da ligação de dados 
  char temp[3]; // guarda a porta no tipo string
  itoa(porta,temp); 
  char modemDevise [9 + strlen(temp)]; // nome do modem devise p.e /dev/ttyS10
  
  //construção do string que contem o path do modem device
  strcpy(modemDevise,MODEMDEVICE);
  strcat(modemDevise,temp);

  fd = open(modemDevise, O_RDWR | O_NOCTTY );
  if (fd <0) {perror(modemDevise); exit(-1); }

  if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
    perror("tcgetattr");
    exit(-1);
  }
  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
  newtio.c_cc[VMIN]     = 0;   /* blocking read until 0 chars received */

  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) pr�ximo(s) caracter(es) 
  */

  tcflush(fd, TCIOFLUSH);

  if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  printf("New termios structure set\n");

  (void) signal(SIGALRM, atende); // redireciona todos os SIGALRM para a função atende

  // envia trama SET 
  sendFrame_S_U(fd,AC,SET);
  printf("Sended: SET\n");
  alarm(TEMPO_ESPERA); // cria um alarm para gerar os time outs
  
  unsigned char readed; // variavel que vai guardando a informação byte a byte da trama recebida

  enum State actualState = START; // estado inical da maquina de estados 

  // Logical Connection
  while (actualState != STOP)
  {
    read(fd,&readed,1); //lê um byte da informação recebida
    
    if (num_tentativas > MAX_TENTATIVAS){ // se excedeu o limite maximo de tentativas
      //repor os valores standart para não afetar outras funções
      num_tentativas = 0; 
      time_out = FALSE;
      printf("Error llopen: No answer from receiver, closing connection.\n");
      if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) { 
        perror("tcsetattr");
        exit(-1);
      }
      close(fd);
      return -1;
    }
    // envia outra vez a informação de SET caso tenha ocorrido time out 
    if (time_out){ 
      sendFrame_S_U(fd,AC,SET);
      printf("Sended: SET\n");
      alarm(TEMPO_ESPERA);
      time_out = FALSE;
    }
    
    // maquina de estados que valida a informação recebida 
    switch (actualState)
    {
    case START:{ // recebe byte FLAG
      if (readed == FLAG)
        actualState = FLAG_RCV;
      break;
    }
    case FLAG_RCV:{ // recebe byte AC
      if (readed== AC)
        actualState = A_RCV;
      else if (readed != FLAG)
        actualState = START;
      break;
    }  
    case A_RCV:{ // recebe byte UA
      if(readed == UA)
        actualState = C_RCV;
      else if(readed == FLAG)
        actualState = FLAG_RCV;
      else
        actualState = START;    
      break;
    }
    case C_RCV:{ // recebe byte BCC
      if(readed == (AC ^ UA))
        actualState = BCC_OK;
      else if(readed == FLAG)
        actualState = FLAG_RCV;
      else
        actualState = START;  
      break;
    }
    case BCC_OK:{ // recebe byte FLAG
      if (readed == FLAG)
        actualState = STOP;
      else
        actualState = START;
      break;
    }
    default:
      break;
    }
  }
  //repor o numero de tentativas para não afetar outras funções;
  num_tentativas = 0;
  printf("Received: UA\n");
  alarm(DESLIGAR_ALARME); //desliga o alarme porque já foi recebida a resposta pretendida
  
  printf("Logical Connection...Done\n");

  return fd;
}

/**
 * escreve o conteudo do buffer em fd
 * @param fd identificador da ligação de dados
 * @param buffer array de caracteres a transmitir
 * @param length tamanho do array buffer
 * @return numero de caracteres escritos ou -1 em caso de erro
 **/
int llwrite(int fd,char * buffer,int length){
  int trama_length = 6; // tamanho da trama em bytes, não inclui o tamanho dos dados

  char new_buffer[length*2]; //guarda o conteudo de buffer após o mecanismo de byte stuffing 
  
  char BCC_Dados[2]; // array que irá conter os valores de BCC (o array tem tamanho 2 pois o BCC pode ser afetado pelo byte stuffing)
  BCC_Dados[0] = NEUTROXOR;
  BCC_Dados[1] = NEUTROXOR;

  int new_buffer_size = byteStuffing(buffer,length,new_buffer,BCC_Dados);
  trama_length += new_buffer_size;

  if (BCC_Dados[1]!=NEUTROXOR){ // se o BCC foi afetado pelo byte stuffing então o tamanho dele aumenta em um byte
    trama_length++;
  }

  char trama[trama_length]; //array que vai guardar a trama a ser enviada pelo emissor

  //contrução da trama I a ser enviado pelo emissor
  trama[0] = FLAG;
  trama[1] = AC;
  if (Ns)
    trama[2] = I1;
  else
    trama[2] = I0;
  trama[3] = AC ^ trama[2];

  for (int i = 4,t=0; t < new_buffer_size; i++,t++){
    trama[i] = new_buffer[t];
  }
  
  
  // se o BCC foi afetado pelo byte stuffing então é necessário colocar os dois bytes dele na trama
  if (BCC_Dados[1]!=NEUTROXOR){
    trama[4+new_buffer_size] = BCC_Dados[0];
    trama[5+new_buffer_size] = BCC_Dados[1];
    trama[6+new_buffer_size] = FLAG;
  }else{
    trama[4+new_buffer_size] = BCC_Dados[0];
    trama[5+new_buffer_size] = FLAG;
  }
  
  sendFrame_I(fd,trama,trama_length); //envia a informação para o receptor
  printf("Sended: Ns=%i\n",Ns);
  alarm(TEMPO_ESPERA); // cria um alarme para gerar os time outs
  
  unsigned char readed; // variavel que vai guardar a informação envia pelo receiver byte a byte

  enum State actualState = START; // estado inicial da maquina de estados

  enum C answer ; // indica qual o tipo de resposta que o receiver enviou

  //aguardar por uma resposta do receiver
  while (actualState != STOP)
  {
    read(fd,&readed,1); // lê um byte da informação enviada pelo receiver

    if (num_tentativas > MAX_TENTATIVAS){ // se excedeu o numero maximo de tentativas
      //repor os valores standart para não afetar outras funções
      num_tentativas = 0; 
      time_out = FALSE;
      printf("Error llwrite: No answer from receiver.\n");
      return -1;
    }
    
    if (time_out){ // se ocorer um time out o sender irá enviar novamente a informação
      sendFrame_I(fd,trama,trama_length);
      printf("Sended: Ns=%i\n",Ns);
      alarm(TEMPO_ESPERA);
      time_out = FALSE;
    }

    // maquina de estados que valida a informação recebida 
    switch (actualState)
    {
    case START:{ // recebe byte FLAG
      if (readed == FLAG)
        actualState = FLAG_RCV;
      break;
    }
    case FLAG_RCV:{ // recebe byte AC
      if (readed== AC)
        actualState = A_RCV;
      else if (readed != FLAG)
        actualState = START;
      break;
    }  
    case A_RCV:{ //recebe byte RR1, RR0, REJ0, REJ1
      answer = readed; // coloca o valor de readed em answer para ser utilizada nos estados seguintes
      if ( (readed == RR1) || (readed == RR0) || (readed == REJ0) || (readed == REJ1) )
        actualState = C_RCV;
      else if(readed == FLAG)
        actualState = FLAG_RCV;
      else
        actualState = START; 
      break;
    }
    case C_RCV:{ // recebe byte BCC
      if (readed == (AC^answer))
        actualState = BCC_OK;
      else if(readed == FLAG)
        actualState = FLAG_RCV;
      else
        actualState = START;  
      break;
    }
    case BCC_OK:{ // recebe byte FLAG
      if (readed == FLAG){
        if (Nr){
          if(answer == RR1) 
            actualState = STOP;
          else if (answer == REJ0){
            num_tentativas = 0; //repor o numero de tentativas 
            printf("Received: REJ0\n");
            sendFrame_I(fd,trama,trama_length);
            printf("Sended: Ns=%i\n",Ns);
            alarm(DESLIGAR_ALARME);
            alarm(TEMPO_ESPERA);
            actualState = START;
          }
        }else{
          if(answer == RR0)
            actualState = STOP;
          else if (answer == REJ1){
            num_tentativas = 0; //repor o numero de tentativas 
            printf("Received: REJ1\n");
            sendFrame_I(fd,trama,trama_length);
            printf("Sended: Ns=%i\n",Ns);
            alarm(DESLIGAR_ALARME);
            alarm(TEMPO_ESPERA);
            actualState = START;
          }
        }
      }
      else
        actualState = START;
      break;
    }
    default:
      break;
    }
  }  
  printf("Received: Nr=%i\n",Nr);
  //repor o numero de tentativas para não afetar outras funções;
  num_tentativas = 0;
  alarm(DESLIGAR_ALARME); //desliga o alarme porque já foi recebida a resposta pretendida

  changeNSS(); //altera os valores de Ns e Nr 
  
  return trama_length;
}

/**
 * fecha o identificador de ligação de dados 
 * @param fd identificador da ligação de dados
 * @return valor 1 em caso de sucesso e -1 caso contrário
 **/
int llclose(int fd){

  //envia DISC ao receptor
  sendFrame_S_U(fd,AC,DISC);
  printf("Sended: DISC\n");
  alarm(TEMPO_ESPERA); // cria um alarme gera os time outs

  unsigned char readed; // guarda a informação da trama recebida pelo receiver byte a byte

  enum State actualState = START; // estado inicial da maquina de estados 

  //aguardar por uma respota válida do receptor
  while (actualState != STOP)
  {
    read(fd,&readed,1); // lê um byte da informação recebida

    if (num_tentativas > MAX_TENTATIVAS){ // caso exceda o limite maximo de tentativas
      //repor os valores standart para não afetar outras funções
      num_tentativas = 0; 
      time_out = FALSE;
      printf("Error llclose: No answer from receiver, closing connection.\n");
      if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
      }
      close(fd);
      return -1;
    }

    if (time_out){ // se ocorer um time out o sender irá enviar novamente a informação
      sendFrame_S_U(fd,AC,DISC);
      printf("Sended: DISC\n");
      alarm(TEMPO_ESPERA);
      time_out = FALSE;
    }
    
    // maquina de estados que valida a informação recebida 
    switch (actualState)
    {
    case START:{ // recebe byte FLAG
      if (readed == FLAG) 
        actualState = FLAG_RCV;
      break;
    }
    case FLAG_RCV:{ // recebe byte AR
      if (readed== AR)
        actualState = A_RCV;
      else if (readed != FLAG)
        actualState = START;
      break;
    }  
    case A_RCV:{ // recebe byte DISC
      if(readed == DISC)
        actualState = C_RCV;
      else if(readed == FLAG)
        actualState = FLAG_RCV;
      else
        actualState = START;    
      break;
    }
    case C_RCV:{ // recebe byte BCC
      if(readed == (AR ^ DISC))
        actualState = BCC_OK;
      else if(readed == FLAG)
        actualState = FLAG_RCV;
      else
        actualState = START;  
      break;
    }
    case BCC_OK:{ // recebe byte FLAG
      if (readed == FLAG)
        actualState = STOP;
      else
        actualState = START;
      break;
    }
    default:
      break;
    }
  }
  printf("Received: DISC\n");
  alarm(DESLIGAR_ALARME); // como não ocorreu nenhum erro é necessário desligar o alarme

  // envia a trama com a resposta UA 
  sendFrame_S_U(fd,AR,UA);
  printf("Sended: UA\n");

  sleep(3); // importante para garantir que o receptor leia a trama UA antes que o emissor fechar a ligação

  if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }
  
  printf("Closing Connection...\n");
  
  close(fd);

  return 1;
}