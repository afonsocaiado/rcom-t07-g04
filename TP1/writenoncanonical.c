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

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

//volatile int STOP=FALSE;

enum A { AC = 0x03 , AR = 0x01 };
enum State { START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP};
enum C { SET = 0x03, DISC = 0x0b, UA = 0x07, RR0 = 0x05, RR1 = 0x85, REJ0 = 0x01, REJ1 = 0x81 , I0 = 0x00, I1 = 0x40};

const unsigned char FLAG = 0x7e;

int Ns = 0; // numero de sequencia da trama do emisor
int Nr = 1; // numero de sequencia da trama do receptor

int time_out =FALSE;
int num_tentativas = 0; // numero de tentativas de restransmissão

void atende()                   // atende alarme
{
	printf("alarme\n");
  time_out = TRUE;
  num_tentativas++;
  alarm(3);
}

// altera os valores de Ns e Nr
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

int llwrite(int fd,char * buffer,int length){
  int trama_length = 6; // tamanho da trama em bytes, não inclui o tamanho dos dados

  char new_buffer[length*2]; //guarda o conteudo de buffer após o mecanismo de byte stuffing 
  int j=0; // variavel utilizada como indice no array new_buffer
  
  char BCC_Dados = 0x00; // 0x00 é o elemento neutro do XOR
  char temp[2]; // array utilizado quando o BCC é igual a flag ou ao octeto de escape
  int BCC_Affected = 0; // variavel boleana que indica se o BCC foi afetado pelo byte stuffing

  //byte stuffing
  for (int i = 0; i < length; i++)
  {
    if (buffer[i] == 0x7e){
      BCC_Dados = BCC_Dados ^ 0x7e;
      new_buffer[j] = 0x7d;
      new_buffer[j+1] = 0x5e;
      j += 2;
    }else if (buffer[i] == 0x7d){
      BCC_Dados = BCC_Dados ^ 0x7d;
      new_buffer[j] = 0x7d;
      new_buffer[j+1] = 0x5d;
      j += 2;
    }else{
      BCC_Dados = BCC_Dados ^ buffer[i];
      new_buffer[j] = buffer[i];
      j++;
    }
  }

  trama_length += j;
  
  //verificar se o BCC foi afetado pelo byte stuffing
  if(BCC_Dados == 0x7e){
    temp[0] = 0x7d;
    temp[1] = 0x5e;
    BCC_Affected = 1;
    trama_length++;
  }else if (BCC_Dados == 0x7d){
    temp[0] = 0x7d;
    temp[1] = 0x5d;
    BCC_Affected = 1;
    trama_length++;
  }

  char trama[trama_length]; //array que vai guardar a trama a ser enviada pelo emissor

  //comando I a ser enviado pelo emisor
  trama[0] = FLAG;
  trama[1] = AC;
  if (Ns)
    trama[2] = I1;
  else
    trama[2] = I0;
  trama[3] = AC ^ 0x00;

  for (size_t i = 4,t=0; t < j; i++,t++){
    trama[i] = new_buffer[t];    
  }
  
  if (BCC_Affected){
    trama[4+j] = temp[0];
    trama[5+j] = temp[1];
    trama[6+j] = FLAG;
  }else{
    trama[4+j] = BCC_Dados;
    trama[5+j] = FLAG;
  }
  
  write(fd,&trama,trama_length);
  alarm(3);

  unsigned char readed;

  enum State actualState = START;

  //aguardar por uma resposta do receptor
  while (actualState != STOP)
  {
    read(fd,&readed,1);

    if (num_tentativas == 4)
      return -1;

    if (time_out && actualState== START){
      write(fd,&trama,trama_length);
      time_out = FALSE;
    }

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
      if(Nr){
        if(readed == RR1 || readed == REJ0)
          actualState = C_RCV;
        else if(readed == FLAG)
          actualState = FLAG_RCV;
        else
          actualState = START;    
      }else{
        if(readed == RR0 || readed == REJ1)
          actualState = C_RCV;
        else if(readed == FLAG)
          actualState = FLAG_RCV;
        else
          actualState = START;    
      }
      break;
    }
    case C_RCV:{
      if (Nr){
        if(readed == (AC ^ RR1))
          actualState = BCC_OK;
        else if (readed = (AC ^ REJ0)){
          write(fd,&trama,trama_length);
          actualState = START;
        }
        else if(readed == FLAG)
          actualState = FLAG_RCV;
        else
          actualState = START;  
        
      }else{
        if(readed == (AC ^ RR0))
          actualState = BCC_OK;
        else if (readed = (AC ^ REJ1)){
          write(fd,&trama,trama_length);
          actualState = START;
        }
        else if(readed == FLAG)
          actualState = FLAG_RCV;
        else
          actualState = START;  
      }
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
  
  alarm(0); //desliga o alarme porque já foi recebida a resposta pretendida

  changeNSS(); 

  return trama_length;
}


int llclose(int fd){

  char trama[5]; // array que contém os parametros de uma trama 
  
  //comando DISC a ser enviado pelo emisor
  trama[0] = FLAG;
  trama[1] = AC;
  trama[2] = DISC;
  trama[3] = AC ^ DISC;
  trama[4] = FLAG;

  write(fd,&trama,sizeof(trama));
  alarm(3);

  unsigned char readed;

  enum State actualState = START;

  //aguardar por uma respota válida do receptor
  while (actualState != STOP)
  {
    read(fd,&readed,1);

    if (num_tentativas == 4)
      return -1;

    if (time_out && actualState== START){
      write(fd,&trama,sizeof(trama));
      time_out = FALSE;
    }
      
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
  alarm(0);

  //resposta ao comando DISC enviado pelo receptor
  trama[1] = AR;
  trama[2] = UA;
  trama[3] = AR ^UA;

  write(fd,&trama,sizeof(trama));

  return 1;
}

int main(int argc, char** argv)
{
  int fd,c, res;
  struct termios oldtio,newtio;
  char buf[255];
  int i, sum = 0, speed = 0;
  
  /*
  if ( (argc < 2) || 
        ((strcmp("/dev/ttyS0", argv[1])!=0) && 
        (strcmp("/dev/ttyS1", argv[1])!=0) )) {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
    exit(1);
  }*/


  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */


  fd = open(argv[1], O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd <0) {perror(argv[1]); exit(-1); }

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
  newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */

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

  (void) signal(SIGALRM, atende);

  // comandos: SET, I , DISC  --- resposta: UA, RR, REJ
  
  unsigned char BCC = AC ^ SET;

  unsigned char buffer[5];
  buffer[0] = FLAG;
  buffer[1]= AC;
  buffer[2] = SET;
  buffer[3] = BCC;
  buffer[4] = FLAG;

  write(fd,&buffer,sizeof(buffer));
  alarm(3);
  
  unsigned char readed;

  enum State actualState = START;

  // Logical Connection
  while (actualState != STOP)
  {
    read(fd,&readed,1);

    if (num_tentativas == 4)
      return -1;

    if (time_out && actualState== START){
      write(fd,&buffer,sizeof(buffer));
      time_out = FALSE;
    }
      
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
  alarm(0); //desliga o alarme porque já foi recebida a resposta pretendida
  
  sleep(1);
  
  if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  close(fd);
  return 0;
}
