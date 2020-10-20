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

//volatile int STOP=FALSE;

enum A { AR = 0x03 , AC = 0x01 };
enum State { START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP};
enum C { SET = 0x03, UA = 0x07 };

const unsigned char FLAG = 0x7e;

int main(int argc, char** argv)
{
    int fd,c, res;
    struct termios oldtio,newtio;

    /*
    if ( (argc < 2) || 
  	     ((strcmp("/dev/ttyS0", argv[1])!=0) && 
  	      (strcmp("/dev/ttyS1", argv[1])!=0) )){
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
      exit(1);
    }*/


  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */
  
    
    fd = open(argv[1], O_RDWR | O_NOCTTY );
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

  
    // comandos: SET, I , DISC  --- resposta: UA, RR, REJ

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
        if (readed== AR)
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

    unsigned char BCC = AR ^ UA; 

    unsigned char buffer[5];
    buffer[0] = FLAG;
    buffer[1] = AR;
    buffer[2] = UA;
    buffer[3] = BCC;
    buffer[4] = FLAG;

    printf("Press a key...\n");
	  getc(stdin);
  
    res = write(fd, buffer, sizeof(buffer));

    printf("%d bytes written\n", res);
	  // write string back to sender
	  //write(fd, msg, strlen(msg)+1);


  /* 
    O ciclo WHILE deve ser alterado de modo a respeitar o indicado no gui�o 
  */



    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}
