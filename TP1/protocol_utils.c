#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1
#define MODEMDEVICE "/dev/ttyS"

#define ESC 0x7D
#define ESCFLAG 0x5E
#define ESCESC 0x5D
#define NEUTROXOR 0x00  // 0x00 é o elemento neutro do XOR

enum State { START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, ESC_STATE, STOP}; // estados possiveis da maquina de estados usada
enum C { SET = 0x03, UA = 0x07, DISC = 0x0b, I0 = 0x00, I1 = 0x40, RR0 = 0x05, RR1 = 0x85, REJ0 = 0x01, REJ1 = 0x81}; // valores possiveis do campo C na trama

const unsigned char FLAG = 0x7e; // flag de inicio e fim de uma trama

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
* função que envia tramas de supervisão (S) e não numeradas (U)
* @param fd identificador da ligacao de dados
* @param campo_endereco campo de endereco da trama a enviar
* @param campo_controlo campo de controlo da trama a enviar
**/
void sendFrame_S_U(int fd, unsigned char campo_endereco, unsigned char campo_controlo){
  unsigned char buffer[5]; // array que vai guardar a trama a ser enviada
  buffer[0] = FLAG;
  buffer[1] = campo_endereco;
  buffer[2] = campo_controlo;
  buffer[3] = campo_endereco ^ campo_controlo;
  buffer[4] = FLAG;

  write(fd,buffer,sizeof(buffer));
}