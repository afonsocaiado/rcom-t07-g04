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

volatile int STOP=FALSE;

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

  
    unsigned char F = 0x7e;
    unsigned char AR = 0x03; //resposta
    unsigned char AC = 0x01; //comando
    unsigned char SET = 0x03;
    
    // comandos: SET, I , DISC  --- resposta: UA, RR, REJ

    unsigned char readed;

    int start = 0;
    while (!start)
    {
      while (readed != F) {       /* wayting for frame start */
        read(fd,&readed,1);
        printf("\nFlag\n");
      }

      read(fd,&readed,1);
      read(fd,&readed,1);
      if (readed == SET){
        printf("\nSet Up\n");
        start= 1;
      }
        
      read(fd,&readed,1);
      read(fd,&readed,1);
      if (readed == F)
        printf("\nFlag\n");
    }

    unsigned char UA = 0x07;
    unsigned char BCC = AR ^ UA; 

    unsigned char buffer[5];
    buffer[0] = F;
    buffer[1] = AR;
    buffer[2] = UA;
    buffer[3] = BCC;
    buffer[4] = F;

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
