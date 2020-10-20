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
enum C { SET = 0x03, UA = 0x07 };

const unsigned char FLAG = 0x7e;

int time_out =FALSE;

void atende()                   // atende alarme
{
	printf("alarme\n");
  time_out = TRUE;
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

    (void) signal(SIGALRM, atende);

    // comandos: SET, I , DISC  --- resposta: UA, RR, REJ

    unsigned char BCC = AC ^ SET;

    unsigned char buffer[5];
    buffer[0] = FLAG;
    buffer[1]= AC;
    buffer[2] = SET;
    buffer[3] = BCC;
    buffer[4] = FLAG;

    res = write(fd,buffer,sizeof(buffer));
    alarm(3);
   
    printf("%d bytes written\n", res);

    unsigned char readed;

    enum State actualState = START;

    // Logical Connection
    while (actualState != STOP)
    {
      read(fd,&readed,1);
  
      if (time_out && actualState== START)
        write(fd,buffer,sizeof(buffer));
      
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
    
  printf("Logical Connection Completed!\n");

  /* 
    O ciclo FOR e as instru��es seguintes devem ser alterados de modo a respeitar 
    o indicado no gui�o 
  */

    sleep(1);
   
    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    close(fd);
    return 0;
}
