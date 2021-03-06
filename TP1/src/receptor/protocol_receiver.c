#include <sys/types.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

#include "../protocol_utils.c"

enum A { AR = 0x03 , AC = 0x01 }; // valores possiveis do campo A na trama

speed_t BAUDRATE; // baudrate que irá ser utilizado na ligação série 
int esperado = 0; //numero de sequencia esperado
int Delay; //atraso introduzido no processamento dos dados no llread
int FER; //frame error ratio
struct termios oldtio,newtio; //variaveis utilizadas para guardar a configuracao da ligacao pelo cabo serie


/**
 * função que vai gerar erros na trama ou atrasos no processamento de cada trama recebida 
 * @param readed variavel que contem um byte lido da trama que irá sofrer alteração forçada para gerar erros
 */ 
void testEfficiency(unsigned char * readed){
  //gerar um numero de 0 a 100
  srand(time(NULL));
  int number = rand() %100;

  if (FER != 0){// se o FER não for 0
    //o readed só vai ver alterado se number tiver um valor menor que FER*100
    if (number < FER){
      *readed = 0x00;
    }
  } 
  if (Delay != 0){ // se o delay não for 0
    if (number <30){
      sleep(1); //atraso de 1 segundo
    }
  }
}

/**
 * funcao responsavel por abir a ligação série, ler a trama SET e devolver a trama UA
 * @param porta porta série p.e 0, 1, 10, 11
 * @return retorna o identificador de ligação
 **/
int llopen(int porta) {
  int fd; //identificador de ligação
  char temp[3]; // guarda a porta no tipo string
  itoa(porta,temp); 
  char modemDevise [9 + strlen(temp)]; // nome do modem devise p.e /dev/ttyS10
  
  //construção do string que contem o path do modem device
  strcpy(modemDevise,MODEMDEVICE);
  strcat(modemDevise,temp);

  fd = open(modemDevise, O_RDWR | O_NOCTTY);
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
  newtio.c_cc[VMIN]     = 1;   /* blocking read until 1 chars received */

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

  unsigned char readed; // variavel que guarda a informacao recebida byte a byte
  enum State actualState = START; // estado inicial da maquina de estados

  // Logical Connection
  while (actualState != STOP)
  {
    read(fd,&readed,1); //le um byte da informacao recebida
    switch (actualState) // maquina de estados que valida a informacao recebida
    {
    case START:{ // recebe byte FLAG
      if (readed == FLAG) 
        actualState = FLAG_RCV;
      break;
    }
    case FLAG_RCV:{ // recebe byte AR
      if (readed == AR) 
        actualState = A_RCV;
      else if (readed != FLAG)
        actualState = START;
      break;
    }  
    case A_RCV:{ // recebe byte SET
      if(readed == SET) 
        actualState = C_RCV;
      else if(readed == FLAG) 
        actualState = FLAG_RCV;
      else
        actualState = START;    
      break;
    }
    case C_RCV:{ // recebe byte BCC
      if(readed == (AR ^ SET))
        actualState = BCC_OK;
      else if(readed == FLAG) 
        actualState = FLAG_RCV;
      else
        actualState = START;  
      break;
    }
    case BCC_OK:{ // recebe byte FLAG final
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
  printf("Received: SET\n");

  //responde ao emissor com UA
  sendFrame_S_U(fd,AR,UA);
  printf("Sended: UA\n");

  return fd;
}

/**
* funcao que le as tramas de informacao e faz destuffing
* @param fd identificador da ligacao de dados
* @param message array que irá receber a informação transmitida pelo emissor
* @return retorna o numero de bytes recebidos ou -1 em caso de erro
**/
int llread(int fd,unsigned char * message) {
  int size = 0; //tamanho da mensagem que está a ser recebida
  unsigned char c_read; // variavel para guardar o byte do campo de controlo
  int trama = 0; // variavel que varia consoante o valor de N(s) recebido
  int mandarDados = FALSE; // variavel que esta a TRUE quando o BCC foi corretamente recebido no final da trama
  char BCC_DADOS = NEUTROXOR; //guarda o valor do XOR entre todos os dados mais o BCC dos dados

  unsigned char readed; // variavel que guarda a informacao recebida byte a byte
  enum State actualState = START; // estado inicial da maquina de estados

  // Recebe a trama de I(0) ou I(1)
  while (actualState != STOP)
  {
    read(fd,&readed,1); //le um byte da informacao recebida
    testEfficiency(&readed);
    switch (actualState) // maquina de estados que valida a informacao recebida
    {
    case START:{ // recebe byte FLAG
      if (readed == FLAG)
        actualState = FLAG_RCV;
      break;
    }
    case FLAG_RCV:{ // recebe byte AR
      if (readed == AR)
        actualState = A_RCV;
      else if (readed != FLAG)
        actualState = START;
      break;
    }  
    case A_RCV:{ // recebe byte I0 ou I1
      if(readed == I0){ // se o numero de sequencia N(s) é 0 
        actualState = C_RCV;
        c_read = readed;
        trama = 0;
      }
      else if(readed == I1){ // se o numero de sequencia N(s) é 1
        actualState = C_RCV;
        c_read = readed;
        trama = 1;
      }
      else if(readed == FLAG)
        actualState = FLAG_RCV;
      else
        actualState = START;    
      break;
    }
    case C_RCV:{ // recebe byte BCC
      if(readed == (AR ^ c_read))
        actualState = BCC_OK;
      else if(readed == FLAG)
        actualState = FLAG_RCV;
      else
        actualState = START;  
      break;
    }
    case BCC_OK:{
      if (readed == FLAG){ // se recebe FLAG final
        if (BCC_DADOS == 0x00) // se o BCC recebido esta correto (0x00 significa que o XOR dos dados é igual ao BCC)
        {
          if (trama == 0){ // se N(s) foi 0
            sendFrame_S_U(fd, AR ,RR1); // responde ao emissor com confirmacao positiva e com N(r) = 1
            printf("Sended RR, T: %d\n", trama^1);
          }else { // se N(s) foi 1
            sendFrame_S_U(fd, AR, RR0); // responde ao emissor com confirmacao positiva e com N(r) = 0
            printf("Sended RR, T: %d\n", trama^1);
          }
          actualState = STOP;
          mandarDados = TRUE;
        }
        else // se o BCC recebido nao esta correto
        {
          if (trama == 0) // se N(s) foi 0
            sendFrame_S_U(fd, AR, REJ0); // responde ao emissor com confirmacao negativa e com N(r) = 0 
          else // se N(s) foi 1
            sendFrame_S_U(fd, AR, REJ1); // responde ao emissor com confirmacao negativa e com N(r) = 1
          
          actualState = STOP;
          mandarDados = FALSE;
          printf("Sended REJ, T: %d\n", trama);
        }
      }
      else if (readed == ESC) // se recebe o octeto de escape
        actualState = ESC_STATE;
      else { // se não for nenhum dos anteriores significa que se trata de um byte de dados
        BCC_DADOS = BCC_DADOS ^ readed;
        message[size] = readed;
        size++;
      }
      break;
    }
    case ESC_STATE:{ // recebeu octeto de escape
      if (readed == ESCFLAG) // se apos o octeto de escape, a sequencia se seguir com 0x5e
      { 
        BCC_DADOS = BCC_DADOS ^ FLAG;
        message[size] = FLAG;
        size++;
      }
      else
      {
        if (readed == ESCESC) // se apos o octeto de escape, a sequencia se seguir com 0x5d
        {
          BCC_DADOS = BCC_DADOS ^ ESC;
          message[size] = ESC;
          size++;
        }
        else // neste caso a sequencia apos o octeto de escape nao e valida
        {
          perror("Non valid character after escape character\n");
          exit(-1);
        }
      }
      actualState = BCC_OK; // volta para o estado em que espera pela leitura da FLAG final
      break;
    }
    default:
      break;
    }
  }
  
  //message tem BCC2 no fim
  size = size - 1;
  printf("Message size: %d\n", size);
  
  if (mandarDados) // se o BCC foi valido
  {
    if (trama == esperado)
    {
      esperado ^= 1;
    }
    else
      size = 0;
  }
  else
    size = 0;

  return size;
}

/**
* funcao que le as tramas de controlo DISC, responde ao emissor com DISC, e recebe a trama UA
* @param fd identificador da ligacao de dados
* @return retorna 1 em caso de sucesso e -1 caso contrário
**/
int llclose(int fd) {
  unsigned char readed; // variavel que guarda a informacao recebida byte a byte
  enum State actualState = START; // estado inicial da maquina de estados

  // Recebe a trama de DISC
  while (actualState != STOP)
  {
    read(fd,&readed,1); //le um byte da informacao recebida
    switch (actualState) // maquina de estados que valida a informacao recebida
    {
    case START:{ // recebe byte FLAG
      if (readed == FLAG)
        actualState = FLAG_RCV;
      break;
    }
    case FLAG_RCV:{ // recebe byte AR
      if (readed == AR)
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
    case BCC_OK:{ // recebe byte FLAG final
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

  // responde DISC ao emissor
  sendFrame_S_U(fd,AC,DISC);
  printf("Sended: DISC\n");

  // efetua a leitura da trama UA enviada de volta pelo emissor
  actualState = START; // estado inicial

  // Recebe a trama de UA 
  while (actualState != STOP)
  {
    read(fd,&readed,1); //le um byte da informacao recebida

    switch (actualState) // maquina de estados que valida a informacao recebida
    {
    case START:{ // recebe byte FLAG
      if (readed == FLAG)
        actualState = FLAG_RCV;
      break;
    }
    case FLAG_RCV:{ // recebe byte AC
      if (readed == AC)
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
    case BCC_OK:{ // recebe byte FLAG final
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

  printf("Received: UA\n");
  
  if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }
  
  printf("Receiver terminated\n");
  
  close(fd);

  return 1;
}