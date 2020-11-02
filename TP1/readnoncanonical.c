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
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define ESC 0x7D
#define ESCFLAG 0x5E
#define ESCESC 0x5D

//volatile int STOP=FALSE;

enum A { AR = 0x03 , AC = 0x01 }; // valores possiveis do campo A na trama
enum State { START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, ESC_STATE, STOP}; // estados possiveis da maquina de estados usada
enum C { SET = 0x03, UA = 0x07, DISC = 0x0b, I0 = 0x00, I1 = 0x40, RR0 = 0x05, RR1 = 0x85, REJ0 = 0x01, REJ1 = 0x81}; // valores possiveis do campo C na trama

const unsigned char FLAG = 0x7e; // flag de inicio e fim de uma trama

int time_out =FALSE; // flag que indica se ocorreu um time out
int num_tentativas = 0; // numero de tentativas de restransmissao

int esperado = 0;
struct termios oldtio,newtio; //variaveis utilizadas para guardar a configuracao da ligacao pelo cabo serie

/**
 * função responsavel por lidar com os SIGALARM 
 **/
void atende()
{
  num_tentativas++;
  if (num_tentativas<=3)
	  printf("Time Out...%i\n",num_tentativas);
  time_out = TRUE;
  alarm(3);
}

/**
 * verifica se o BCC recebido está correto
 * @param message mensagem recebida
 * @param size tamanho da mensagem
 * @return TRUE em caso de sucesso, FALSE caso contrario
 **/
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

/**
 * funcao que verifica se a trama recebida é a trama END
 * @param start trama START recebida anteriormente
 * @param sizeStart tamanho da trama START
 * @param end mensagem lida a verificar se é END
 * @param sizeEnd tamanho dessa mesma mensagem
 * @return TRUE se for END, FALSE caso contrario
 **/
int isAtEnd(unsigned char *start, int sizeStart, unsigned char *end, int sizeEnd)
{
  int s = 1;
  int e = 1;
  if (sizeStart != sizeEnd) // se o tamanho da trama a analisar e da trama de START nao for o mesmo
    return FALSE;
  else
  {
    if (end[0] == 0x03) // se o campo de controlo da trama atual for 3 (end)
    {
      for (; s < sizeStart; s++, e++) // percorre a trama atual e a trama START simultaneamente, para as comparar
      {
        if (start[s] != end[e]) // se a trama a analisar nao for igual a trama de START em qualquer um dos bytes
          return FALSE;
      }
      return TRUE;
    }
    else
    {
      return FALSE;
    }
  }
}

/**
 * funcao responsavel por ler a trama SET e devolver a trama UA
 * @param fd identificador da ligacao de dados
 **/
void llopen(int fd) {

    int readSetMessage = 0; // variavel utilizada para auxiliar a verificacao da rececao correta da trama de controlo SET

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

    (void) signal(SIGALRM, atende); // redireciona todos os SIGALRM para a função atende

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
      case A_RCV:{ // recebe byte de Controlo
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
    readSetMessage = 1; // trama SET foi corretamente recebida

    if (readSetMessage == 1) { // como recebeu corretamente a trama SET, comeca o envio da trama UA
      printf("Received SET\n");

      unsigned char buffer[5]; // array que vai guardar a trama a ser enviada
      // comeca a construcao da trama UA a enviar para o emissor
      buffer[0] = FLAG;
      buffer[1] = AR;
      buffer[2] = UA;
      buffer[3] = buffer[1] ^ buffer[2];
      buffer[4] = FLAG;
      write(fd, buffer, sizeof(buffer));

      printf("Sent UA\n");
    }
}

/**
* funcao utilizada para a leitura de tramas de informacao, envia uma trama de controlo
* @param fd identificador da ligacao de dados
* @param answer campo de controlo da trama a enviar
**/
void sendControlMessage(int fd, char answer){
  char buffer[5]; // array que vai guardar a trama a ser enviada
  // comeca a construcao da trama UA a enviar para o emissor
  buffer[0] = FLAG;
  buffer[1] = AR;
  buffer[2] = answer;
  buffer[3] = AR ^ answer;
  buffer[4] = FLAG;

  write(fd,&buffer,5);
}

/**
* funcao que le as tramas de informacao e faz destuffing
* @param fd identificador da ligacao de dados
* @param size tamanho da mensagem recebida
* @return retorna a mensagem recebida
**/
unsigned char *llread(int fd, int *size) {
  unsigned char *message = (unsigned char *)malloc(1); // aloca espaco de memoria para a mensagem a receber
  *size = 0;
  unsigned char c_read; // variavel para guardar o byte do campo de controlo
  int trama = 0; // variavel que varia consoante o valor de N(s) recebido
  int mandarDados = FALSE; // variavel que esta a TRUE quando o BCC foi corretamente recebido no final da trama

  unsigned char readed; // variavel que guarda a informacao recebida byte a byte
  enum State actualState = START; // estado inicial da maquina de estados

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
    case A_RCV:{ // recebe campo de controlo 
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
        if (seeBCC2(message, *size)) // se o BCC recebido esta correto
        {
          if (trama == 0) // se N(s) foi 0
            sendControlMessage(fd, RR1); // responde ao emissor com confirmacao positiva e com N(r) = 1
          else // se N(s) foi 1
            sendControlMessage(fd, RR0); // responde ao emissor com confirmacao positiva e com N(r) = 0

          actualState = STOP;
          mandarDados = TRUE;
          printf("Enviou RR, T: %d\n", trama);
        }
        else // // se o BCC recebido nao esta
        {
          if (trama == 0) // se N(s) foi 0
            sendControlMessage(fd, REJ1); // responde ao emissor com confirmacao negativa e com N(r) = 1
          else // se N(s) foi 1
            sendControlMessage(fd, REJ0); // responde ao emissor com confirmacao negativa e com N(r) = 0
          actualState = STOP;
          mandarDados = FALSE;
          printf("Enviou REJ, T: %d\n", trama);
        }
      }
      else if (readed == ESC) // se recebe o octeto de escape
        actualState = ESC_STATE;
      else { 
        message = (unsigned char *)realloc(message, ++(*size)); 
        message[*size - 1] = readed;
      }
      break;
    }
    case ESC_STATE:{ // recebeu octeto de escape
      if (readed == ESCFLAG) // se apos o octeto de escape, a sequencia se seguir com 0x5e
      {
        message = (unsigned char *)realloc(message, ++(*size));
        message[*size - 1] = FLAG;
      }
      else
      {
        if (readed == ESCESC) // se apos o octeto de escape, a sequencia se seguir com 0x5d
        {
          message = (unsigned char *)realloc(message, ++(*size));
          message[*size - 1] = ESC;
        }
        else // neste caso a sequencia apos o octeto de escape nao e valida
        {
          perror("Non valid character after escape character");
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

  printf("Message size: %d\n", *size);
  //message tem BCC2 no fim
  message = (unsigned char *)realloc(message, *size - 1);

  *size = *size - 1;
  if (mandarDados) // se o BCC foi valido
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

/**
* funcao que le as tramas de controlo DISC, responde ao emissor com DISC, e recebe a trama UA
* @param fd identificador da ligacao de dados
**/
void llclose(int fd) {

    // Recebe a trama de DISC

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
    printf("Recebeu DISC\n");

    // responde DISC ao emissor
    unsigned char buffer[5]; // array que vai guardar a trama a ser enviada
    // comeca a construcao da trama DISC a enviar para o emissor
    buffer[0] = FLAG;
    buffer[1] = AC;
    buffer[2] = DISC;
    buffer[3] = buffer[1] ^ buffer[2];
    buffer[4] = FLAG;
    write(fd, buffer, sizeof(buffer));
    alarm(3);

    printf("Mandou DISC\n");

    // efetua a leitura da trama UA enviada de volta pelo emissor
    actualState = START; // estado inicial

    // Logical Connection
    while (actualState != STOP)
    {
      read(fd,&readed,1); //le um byte da informacao recebida

      if (num_tentativas > 3){ // se excedeu o limite maximo de tentativas
        //repor os valores standart para não afetar outras funções
        num_tentativas = 0; 
        time_out = FALSE;
        printf("Error llclose: No answer from sender, closing connection.\n");
        if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) { 
          perror("tcsetattr");
          exit(-1);
        }
        close(fd);
        return -1;
      }
      // envia outra vez a informação de DISC caso tenha ocorrido time out 
      if (time_out){ 
        write(fd,&buffer,sizeof(buffer));
        printf("Sended: DISC\n");
        time_out = FALSE;
      }

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
      case A_RCV:{ // recebe UA
        if(readed == UA)
          actualState = C_RCV;
        else if(readed == FLAG)
          actualState = FLAG_RCV;
        else
          actualState = START;    
        break;
      }
      case C_RCV:{ // recebe BCC
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

    printf("Recebeu UA\n");
    printf("Receiver terminated\n");
}


// Main function
int main(int argc, char** argv)
{
    int fd; // descritor da ligacao de dados
    unsigned char *mensagemPronta; // variavel onde cada trama vai ser guardada
    int sizeMessage = 0; // tamanho da trama
    int sizeOfStart = 0; // tamanho da trama START
    unsigned char *start; // variavel que guarda a trama START
    unsigned char *giant; // variavel que guarda os dados de todas as tramas de informacao recebidas
    off_t sizeOfGiant = 0; // tamanho da variavel giant
    off_t index = 0; // variavel auxiliar para ajudar a colocar cada trama no local correto de giant


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

    llopen(fd);
  
    start = llread(fd, &sizeOfStart);

    // codigo que obtem o nome do ficheiro a partir da trama START
    int L2 = (int)start[8];
    unsigned char *name = (unsigned char *)malloc(L2 + 1);

    int i;
    for (i = 0; i < L2; i++)
    {
      name[i] = start[9 + i];
    }

    name[L2] = '\0';
    unsigned char *nameOfFile = name;

    sizeOfGiant = (start[3] << 24) | (start[4] << 16) | (start[5] << 8) | (start[6]); // tamanho do ficheiro a partir da trama START

    giant = (unsigned char *)malloc(sizeOfGiant);

    while (TRUE)
    {
      mensagemPronta = llread(fd, &sizeMessage); 
      if (sizeMessage == 0)
        continue;

      if (isAtEnd(start, sizeOfStart, mensagemPronta, sizeMessage)) // se leu a trama de END
      {
        printf("End message received\n");
        break;
      }

      int sizeWithoutHeader = 0;

      // remove o cabeçalho do nível de aplicação das tramas de informacao
      int i = 0;
      int j = 4;
      unsigned char *messageRemovedHeader = (unsigned char *)malloc(sizeMessage - 4);
      for (; i < sizeMessage; i++, j++)
      {
        messageRemovedHeader[i] = mensagemPronta[j];
      }
      sizeWithoutHeader = sizeMessage - 4;
      mensagemPronta = messageRemovedHeader; // mensagem recebida totalmente tratada

      memcpy(giant + index, mensagemPronta, sizeWithoutHeader); // copia a mensagem para giant
      index += sizeWithoutHeader;
    }

    // imprime mensagem apos leitura de todas as tramas
    printf("Mensagem: \n");
    i = 0;
    for (; i < sizeOfGiant; i++)
    {
      printf("%x", giant[i]);
    }

    // cria ficheiro com os dados das tramas de informacao recebidas
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
