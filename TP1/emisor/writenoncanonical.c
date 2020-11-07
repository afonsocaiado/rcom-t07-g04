/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#define MODEMDEVICE "/dev/ttyS"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1
#define DESLIGAR_ALARME 0
#define TEMPO_ESPERA 5
#define MAX_TENTATIVAS 3
#define NUM_BAUND_RATES 19

enum A { AC = 0x03 , AR = 0x01 }; // valores possiveis do campo A na trama
enum State { START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP}; // estados possiveis da maquina de estados usada
enum C { SET = 0x03, DISC = 0x0b, UA = 0x07, RR0 = 0x05, RR1 = 0x85, REJ0 = 0x01, REJ1 = 0x81 , I0 = 0x00, I1 = 0x40};// valores possiveis do campo C na trama

int BAUDRATE = B38400; // baudrate que está por default

struct baud_rate{
  speed_t baud;
  char *bauds;
};

//struct que vai guardar os baudrates para melhor acessibilidade 
struct baud_rate rates[NUM_BAUND_RATES] = {
  {B0,"B0"},
  {B50,"B50"},
  {B75,"B75"},
  {B110,"B110"},
  {B134,"B134"},
  {B150,"B150"},
  {B200,"B200"},
  {B300,"B300"},
  {B600,"B600"},
  {B1200,"B1200"},
  {B1800,"B1800"},
  {B2400,"B2400"},
  {B4800,"B4800"},
  {B9600,"B9600"},
  {B19200,"B19200"},
  {B38400,"B38400"},
  {B57600,"B57600"},
  {B115200,"B115200"},
  {B230400,"B230400"},
};

const unsigned char FLAG = 0x7e; // flag de inicio e fim de uma trama

int Ns = 0; // numero de sequencia da trama do emisor
int Nr = 1; // numero de sequencia da trama do receptor

int time_out =FALSE; // flag que indica se ocorreu um time out
int num_tentativas = 0; // numero de tentativas de restransmissão

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
  alarm(TEMPO_ESPERA);
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
 * converte inteiros para string
 * @param i inteiro a ser convertido
 * @param b array de caracteres destino
 **/ 
void itoa(int i, char b[]){
    char const digit[] = "0123456789";
    char* p = b;
    if(i<0){
        *p++ = '-';
        i *= -1;
    }
    int shifter = i;
    do{ //Move to where representation ends
        ++p;
        shifter = shifter/10;
    }while(shifter);
    *p = '\0';
    do{ //Move back, inserting digits as u go
        *--p = digit[i%10];
        i = i/10;
    }while(i);
}

 /**
  * mecanismo de byte stuffing 
  * @param buffer array de caracteres que vai sofrer byte stuffing
  * @param length tamanho do array buffer
  * @param new_buffer array que irá receber o conteudo do buffer após o byte stuffing
  * @param BCC array que contem o(s) valore(s) do parametro BCC que protege os dados 
  * @return tamanho do new_buffer mais o BCC caso este seja afetado pelo byte stuffing
  **/
int byteStuffing(char * buffer,int length,char new_buffer[],char BCC[]){
  int j=0; // variavel que vai conter o tamanho de new_buffer mais o Bcc, mas também irá funcionar de indice do array new_buffer

  //byte stuffing do buffer
  for (int i = 0; i < length; i++)
  {
    if (buffer[i] == 0x7e){
      BCC[0] = BCC[0] ^ 0x7e;
      new_buffer[j] = 0x7d;
      new_buffer[j+1] = 0x5e;
      j += 2;
    }else if (buffer[i] == 0x7d){
      BCC[0] = BCC[0]^ 0x7d;
      new_buffer[j] = 0x7d;
      new_buffer[j+1] = 0x5d;
      j += 2;
    }else{
      BCC[0] = BCC[0] ^ buffer[i];
      new_buffer[j] = buffer[i];
      j++;
    }
  }

  //verifica se o BCC é afetado pelo byte stuffing
  if(BCC[0] == 0x7e){
    BCC[0] = 0x7d;
    BCC[1] = 0x5e;
  }else if (BCC[0] == 0x7d){
    BCC[0] = 0x7d;
    BCC[1] = 0x5d;
  }
  return j;
}

/**
 * envia a informação para o receptor
 * @param fd identificador da ligação de dados
 * @param trama array de caracteres a enviar 
 * @param trama_length indica o tamanho da trama a enviar 
 **/ 
void sendFrame(int fd, char * trama, int trama_length){

  int bytesSended = 0; // bytes que já foram enviados 
  int indice = 0; // indica o indice da trama onde deve começar a enviar info
  int remaning = trama_length; // indica a quantidade de bytes que falta enviar 
  while (bytesSended != remaning )
  {
    bytesSended = write(fd,&trama[indice],remaning); // envia a informação para o receiver
    if ( (bytesSended != remaning) && (bytesSended != -1) ){
      indice += bytesSended; //atualiza o indice do array 
      remaning -=bytesSended; // decrementa a quantidade de bytes que faltam enviar
    }
    //printf("%i - %i\n",bytesSended,remaning);  
  }
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
  BCC_Dados[0] = 0x00; // 0x00 é o elemento neutro do XOR
  BCC_Dados[1] = 0x00;

  int new_buffer_size = byteStuffing(buffer,length,new_buffer,BCC_Dados);
  trama_length += new_buffer_size;

  if (BCC_Dados[1]!=0x00){ // se o BCC foi afetado pelo byte stuffing então o tamanho dele aumenta em um byte
    trama_length++;
  }

  char trama[trama_length]; //array que vai guardar a trama a ser enviada pelo emissor

  //contrução da trama I a ser enviado pelo emisor
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
  if (BCC_Dados[1]!=0x00){
    trama[4+new_buffer_size] = BCC_Dados[0];
    trama[5+new_buffer_size] = BCC_Dados[1];
    trama[6+new_buffer_size] = FLAG;
  }else{
    trama[4+new_buffer_size] = BCC_Dados[0];
    trama[5+new_buffer_size] = FLAG;
  }
  
  sendFrame(fd,trama,trama_length); //envia a informação para o receptor
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
      sendFrame(fd,trama,trama_length);
      printf("Sended: Ns=%i\n",Ns);
      time_out = FALSE;
    }

    // maquina de estados que valida a informação recebida 
    switch (actualState)
    {
    case START:{
      if (readed == FLAG)
        actualState = FLAG_RCV;
      break;
    }
    case FLAG_RCV:{
      if (readed== AC)
        actualState = A_RCV;
      else if (readed != FLAG)
        actualState = START;
      break;
    }  
    case A_RCV:{
      answer = readed; // coloca o valor de readed em answer para ser utilizada nos estados seguintes
      if ( (readed == RR1) || (readed == RR0) || (readed == REJ0) || (readed == REJ1) )
        actualState = C_RCV;
      else if(readed == FLAG)
        actualState = FLAG_RCV;
      else
        actualState = START; 
      break;
    }
    case C_RCV:{
      if (readed == (AC^answer))
        actualState = BCC_OK;
      else if(readed == FLAG)
        actualState = FLAG_RCV;
      else
        actualState = START;  
      break;
    }
    case BCC_OK:{
      if (readed == FLAG){
        if (Nr){
          if(answer == RR1)
            actualState = STOP;
          else if (answer == REJ0){
            num_tentativas = 0; //repor o numero de tentativas 
            printf("Received: REJ0\n");
            sendFrame(fd,trama,trama_length);
            printf("Sended: Ns=%i\n",Ns);
            actualState = START;
          }
        }else{
          if(answer == RR0)
            actualState = STOP;
          else if (answer == REJ1){
            num_tentativas = 0; //repor o numero de tentativas 
            printf("Received: REJ1\n");
            sendFrame(fd,trama,trama_length);
            printf("Sended: Ns=%i\n",Ns);
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

  changeNSS(); 
  
  return trama_length;
}

/**
 * fecha o identificador de ligação de dados 
 * @param fd identificador da ligação de dados
 * @return valor 1 em caso de sucesso e -1 caso contrário
 **/
int llclose(int fd){

  char trama[5]; // array que contém os parametros de uma trama 
  
  //comando DISC a ser enviado pelo emisor
  trama[0] = FLAG;
  trama[1] = AC;
  trama[2] = DISC;
  trama[3] = AC ^ DISC;
  trama[4] = FLAG;

  sendFrame(fd,trama,sizeof(trama)); // envia a trama com o comando DISC para o receiver
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
      sendFrame(fd,trama,sizeof(trama));
      printf("Sended: DISC\n");
      time_out = FALSE;
    }
    
    // maquina de estados que valida a informação recebida 
    switch (actualState)
    {
    case START:{
      if (readed == FLAG)
        actualState = FLAG_RCV;
      break;
    }
    case FLAG_RCV:{
      if (readed== AR)
        actualState = A_RCV;
      else if (readed != FLAG)
        actualState = START;
      break;
    }  
    case A_RCV:{
      if(readed == DISC)
        actualState = C_RCV;
      else if(readed == FLAG)
        actualState = FLAG_RCV;
      else
        actualState = START;    
      break;
    }
    case C_RCV:{
      if(readed == (AR ^ DISC))
        actualState = BCC_OK;
      else if(readed == FLAG)
        actualState = FLAG_RCV;
      else
        actualState = START;  
      break;
    }
    case BCC_OK:{
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

  //resposta ao comando DISC enviado pelo receptor
  trama[1] = AR;
  trama[2] = UA;
  trama[3] = AR ^UA;

  sendFrame(fd,trama,sizeof(trama)); // envia a trama com a resposta UA 
  //sendFrame(fd,trama,sizeof(trama)); 
  //sendFrame(fd,trama,sizeof(trama)); 
  printf("Sended: UA\n");

  if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }
  
  printf("Closing Connection...\n");

  sleep(3);
  
  close(fd);

  return 1;
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

  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */
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

  // contrução da trama SET 
  unsigned char BCC = AC ^ SET;
  char buffer[5];
  buffer[0] = FLAG;
  buffer[1]= AC;
  buffer[2] = SET;
  buffer[3] = BCC;
  buffer[4] = FLAG;

  sendFrame(fd,buffer,sizeof(buffer)); // envia a informação para o receiver
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
      sendFrame(fd,buffer,sizeof(buffer));
      printf("Sended: SET\n");
      time_out = FALSE;
    }
    
    // maquina de estados que valida a informação recebida 
    switch (actualState)
    {
    case START:{
      if (readed == FLAG)
        actualState = FLAG_RCV;
      break;
    }
    case FLAG_RCV:{
      if (readed== AC)
        actualState = A_RCV;
      else if (readed != FLAG)
        actualState = START;
      break;
    }  
    case A_RCV:{
      if(readed == UA)
        actualState = C_RCV;
      else if(readed == FLAG)
        actualState = FLAG_RCV;
      else
        actualState = START;    
      break;
    }
    case C_RCV:{
      if(readed == (AC ^ UA))
        actualState = BCC_OK;
      else if(readed == FLAG)
        actualState = FLAG_RCV;
      else
        actualState = START;  
      break;
    }
    case BCC_OK:{
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
