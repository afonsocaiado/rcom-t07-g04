/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define ESC 0x7D
#define ESCFLAG 0x5E
#define ESCESC 0x5D

//volatile int STOP=FALSE;

enum A { AR = 0x03 , AC = 0x01 };
enum State { START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, ESC_STATE, STOP};
enum C { SET = 0x03, UA = 0x07, DISC = 0x0b, I0 = 0x00, I1 = 0x40, RR0 = 0x05, RR1 = 0x85, REJ0 = 0x01, REJ1 = 0x81};

const unsigned char FLAG = 0x7e;

int time_out =FALSE;
int num_tentativas = 0;

int esperado = 0;
struct termios oldtio,newtio;

int seeBCC2(unsigned char *message, int size) {

  int i = 1;
  unsigned char BCC2 = message[0];

  for (; i < size - 1; i++)
  {
    BCC2 ^= message[i];
  }
  if (BCC2 == message[size - 1])
  {
    return TRUE;
  }
  else
    return FALSE;
}

void llopen(int fd) {

    int readSetMessage = 0;

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

    unsigned char readed;
    enum State actualState = START;

    // Logical Connection
    while (actualState != STOP)
    {
      read(fd,&readed,1);
      switch (actualState)
      {
      case START:{
        if (readed == FLAG)
          actualState = FLAG_RCV;
        break;
      }
      case FLAG_RCV:{
        if (readed == AR)
          actualState = A_RCV;
        else if (readed != FLAG)
          actualState = START;
        break;
      }  
      case A_RCV:{
        if(readed == SET)
          actualState = C_RCV;
        else if(readed == FLAG)
          actualState = FLAG_RCV;
        else
          actualState = START;    
        break;
      }
      case C_RCV:{
        if(readed == (AR ^ SET))
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
    readSetMessage = 1;

    if (readSetMessage == 1) {
      printf("Received SET");

      unsigned char buffer[5];
      buffer[0] = FLAG;
      buffer[1] = AR;
      buffer[2] = UA;
      buffer[3] = buffer[1] ^ buffer[2];
      buffer[4] = FLAG;
      write(fd, buffer, sizeof(buffer));

      printf("Sent UA");
    }
}

unsigned char *llread(int fd, int *size) {
  unsigned char *message = (unsigned char *)malloc(0);
  *size = 0;
  unsigned char c_read;
  int trama = 0;
  int mandarDados = FALSE;

  unsigned char readed;
  enum State actualState = START;

  // Logical Connection
  while (actualState != STOP)
  {
    read(fd,&readed,1);
    switch (actualState)
    {
    case START:{
      if (readed == FLAG)
        actualState = FLAG_RCV;
      break;
    }
    case FLAG_RCV:{
      if (readed == AR)
        actualState = A_RCV;
      else if (readed != FLAG)
        actualState = START;
      break;
    }  
    case A_RCV:{
      if(readed == I0){
        actualState = C_RCV;
        c_read = readed;
        trama = 0;
      }
      else if(readed == I1){
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
    case C_RCV:{
      if(readed == (AR ^ c_read))
        actualState = BCC_OK;
      else if(readed == FLAG)
        actualState = FLAG_RCV;
      else
        actualState = START;  
      break;
    }
    case BCC_OK:{
      if (readed == FLAG){
        if (seeBCC2(message, *size))
        {
          if (trama == 0)
            sendControlMessage(fd, RR1);
          else
            sendControlMessage(fd, RR0);

          actualState = STOP;
          mandarDados = TRUE;
          printf("Enviou RR, T: %d\n", trama);
        }
        else
        {
          if (trama == 0)
            sendControlMessage(fd, REJ1);
          else
            sendControlMessage(fd, REJ0);
          actualState = STOP;
          mandarDados = FALSE;
          printf("Enviou REJ, T: %d\n", trama);
        }
      }
      else if (readed == ESC)
        actualState = ESC_STATE;
      else {
        message = (unsigned char *)realloc(message, ++(*size));
        message[*size - 1] = readed;
      }
      break;
    }
    case ESC_STATE:{
      if (readed == ESCFLAG)
      {
        message = (unsigned char *)realloc(message, ++(*size));
        message[*size - 1] = FLAG;
      }
      else
      {
        if (readed == ESCESC)
        {
          message = (unsigned char *)realloc(message, ++(*size));
          message[*size - 1] = ESC;
        }
        else
        {
          perror("Non valid character after escape character");
          exit(-1);
        }
      }
      actualState = BCC_OK;
      break;
    }
    default:
      break;
    }
  }

  printf("Message size: %d\n", *size);
  //message tem BCC2 no fim
  message = (unsigned char *)realloc(message, *size - 1);

  *size = *size - 1;
  if (mandarDados)
  {
    if (trama == esperado)
    {
      esperado ^= 1;
    }
    else
      *size = 0;
  }
  else
    *size = 0;
  return message;
}

void llclose(int fd) {

// Wait for disc 

    unsigned char readed;
    enum State actualState = START;

    // Logical Connection
    while (actualState != STOP)
    {
      read(fd,&readed,1);
      switch (actualState)
      {
      case START:{
        if (readed == FLAG)
          actualState = FLAG_RCV;
        break;
      }
      case FLAG_RCV:{
        if (readed == AR)
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
    printf("Recebeu DISC\n");

// Envia DISC
    unsigned char buffer[5];
    buffer[0] = FLAG;
    buffer[1] = AR;
    buffer[2] = DISC;
    buffer[3] = buffer[1] ^ buffer[2];
    buffer[4] = FLAG;
    write(fd, buffer, sizeof(buffer));

    printf("Mandou DISC\n");

//Wait for UA
    unsigned char readed;
    enum State actualState = START;

    // Logical Connection
    while (actualState != STOP)
    {
      read(fd,&readed,1);
      switch (actualState)
      {
      case START:{
        if (readed == FLAG)
          actualState = FLAG_RCV;
        break;
      }
      case FLAG_RCV:{
        if (readed == AR)
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
        if(readed == (AR ^ UA))
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

    printf("Recebeu UA\n");
    printf("Receiver terminated\n");
}


// Main function
int main(int argc, char** argv)
{
    int fd;
    int sizeMessage = 0;
    unsigned char *mensagemPronta;
    int sizeOfStart = 0;
    unsigned char *start;
    off_t sizeOfGiant = 0;
    unsigned char *giant;
    off_t index = 0;


    
    if ( (argc < 2) || 
  	     ((strcmp("/dev/ttyS0", argv[1])!=0) && 
  	      (strcmp("/dev/ttyS1", argv[1])!=0) )){
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
      exit(1);
    }


  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */
  
    
    fd = open(argv[1], O_RDWR | O_NOCTTY );
    if (fd <0) {perror(argv[1]); exit(-1); }

  
    llopen(fd);
    start = llread(fd, &sizeOfStart);

    int L2 = (int)start[8];
    unsigned char *name = (unsigned char *)malloc(L2 + 1);

    int i;
    for (i = 0; i < L2; i++)
    {
      name[i] = start[9 + i];
    }

    name[L2] = '\0';
    unsigned char *nameOfFile = name;

    sizeOfGiant = (start[3] << 24) | (start[4] << 16) | (start[5] << 8) | (start[6]);

    giant = (unsigned char *)malloc(sizeOfGiant);

    while (TRUE)
    {
      mensagemPronta = llread(fd, &sizeMessage);
      if (sizeMessage == 0)
        continue;
      if (isEndMessage(start, sizeOfStart, mensagemPronta, sizeMessage))
      {
        printf("End message received\n");
        break;
      }

      int sizeWithoutHeader = 0;

      mensagemPronta = removeHeader(mensagemPronta, sizeMessage, &sizeWithoutHeader);

      memcpy(giant + index, mensagemPronta, sizeWithoutHeader);
      index += sizeWithoutHeader;
    }

    printf("Mensagem: \n");
    int i = 0;
    for (; i < sizeOfGiant; i++)
    {
      printf("%x", giant[i]);
    }

    FILE *file = fopen((char *)nameOfFile, "wb+");
    fwrite((void *)giant, 1, *nameOfFile, file);
    printf("%zd\n", sizeOfGiant);
    printf("New file created\n");
    fclose(file);

    llclose(fd);

    tcsetattr(fd,TCSANOW,&oldtio);

    sleep(1);
 
    close(fd);
    return 0;



}
