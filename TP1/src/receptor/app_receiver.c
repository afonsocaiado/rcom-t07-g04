#include "protocol_receiver.c"
#include "../app_utils.c"
#include <math.h>
/**
 * funcao que verifica se o pacote recebido é o pacote END
 * @param start pacote START recebida anteriormente
 * @param sizeStart tamanho da pacote START
 * @param end mensagem lida a verificar se é END
 * @param sizeEnd tamanho dessa mesma mensagem
 * @return TRUE se for END, FALSE caso contrario
 **/
int isAtEnd(unsigned char *start, int sizeStart, unsigned char *end, int sizeEnd)
{
  int s = 1;
  int e = 1;
  if (sizeStart != sizeEnd) // se o tamanho do pacote a analisar e do pacote de START nao for o mesmo
    return FALSE;
  else
  {
    if (end[0] == 0x03) // se o campo de controlo do pacote atual for 3 (end)
    {
      for (; s < sizeStart; s++, e++) // percorre o pacote atual e o pacote START simultaneamente, para os comparar
      {
        if (start[s] != end[e]) // se o pacote a analisar nao for igual ao pacote de START em qualquer um dos bytes
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

int main(int argc, char** argv)
{
  int fd,porta; // descritor da ligacao de dados (fd) e porta serie (porta)
  unsigned char mensagemPronta[65540]; // variavel onde cada pacote vai ser guardado
  int sizeMessage = 0; // tamanho do pacote
  int sizeOfStart = 0; // tamanho do pacote START
  unsigned char start[100]; // variavel que guarda o pacote START
  struct timespec before , after; // before vai guardar o tempo de quando começa a receber informação, e after vai guardar o tempo de quando acabar de transferir a informação
  struct shell_inputs arguments; // struct que vai guardar os argumentos introduzidos
  u_int64_t before_ns,after_ns; // vai guardar os tempos com grande precisão
  
  //verifica se pelo menos tem 2 argumentos, e se o segundo começa com /dev/ttyS
  if ( (argc < 2) || (strncmp("/dev/ttyS",argv[1],9) != 0) ){
    printf("Invalid inputs, the template command is:\n./app /dev/ttySx [-B 38400]\n");
    exit(1);
  }

  //vai buscar ao array argv os argumentos introduzidos 
  if (readArgvValues(argc,argv,&arguments,RECEIVER) == -1){
    exit(-1);
  }

  //atribuição dos argumentos lidos
  BAUDRATE = arguments.baudrate;
  Delay = arguments.atraso;
  FER = arguments.FER;
  porta = atoi(&arguments.port[9]);

  fd = llopen(porta);

  // verificar se a porta serie foi aberta com sucesso
  if (fd == -1)
    exit(-1);

  sizeOfStart = llread(fd, start); // lê o pacote de controlo START

  if (sizeOfStart == -1) //se ocorreu algum erro na leitura do pacote START
    exit(-1);

  //recolher a informação do tamanho do ficheiro
  int num_blocos_tamanho = start[2];
  char tamanho_str[num_blocos_tamanho];
  for (size_t i = 0; i < num_blocos_tamanho; i++)
  {
    tamanho_str[i] = start[3+i];
  }
  int tamanho_int = atoi(tamanho_str); //converter o tamanho de string para inteiro

  //recolher a informação do nome do ficheiro
  int num_blocos_nome = start[3+num_blocos_tamanho+1];
  char nome_ficheiro[num_blocos_nome+1];
  for (size_t i = 0; i < num_blocos_nome; i++)
  {
    nome_ficheiro[i] = start[5 + num_blocos_tamanho + i];
  }
  nome_ficheiro[num_blocos_nome]= '\0';

  // cria ficheiro com o nome obtido no pacote START
  FILE *file = fopen(nome_ficheiro, "wb+");
  
  clock_gettime(CLOCK_MONOTONIC,&before);//guarda o tempo atual em before

  //enquanto houver informação para ler
  while (TRUE)
  {
    sizeMessage = llread(fd, mensagemPronta); //lê de fd um pacote de informação
   
    if(sizeMessage == -1) // se ocorreu algum erro no llread
      exit(-1);
    if (sizeMessage == 0)
      continue;

    if (isAtEnd(start, sizeOfStart, mensagemPronta, sizeMessage)) // se leu a trama de END
    {
      clock_gettime(CLOCK_MONOTONIC,&after);//guarda o tempo atual em after
      printf("End message received\n");
      break;
    }

    int sizeWithoutHeader = sizeMessage - 4; //guarda o tamanho do pacote de dados sem o cabeçalho 

    // remove o cabeçalho do nível de aplicação dos pacotes de dados
    int i = 0;
    int j = 4;
    unsigned char messageRemovedHeader[sizeWithoutHeader]; //array que vai guardar os dados recebidos
    for (; i < sizeMessage; i++, j++)
    {
      messageRemovedHeader[i] = mensagemPronta[j];
    }
    
    //escreve no ficheiro os dados recebidos 
    fwrite(messageRemovedHeader, 1, sizeWithoutHeader, file);
  }
  
  //pré calculo do tempo decorrido com maior precisão 
  before_ns = (before.tv_sec * 1000000000) + before.tv_nsec;
  after_ns = (after.tv_sec * 1000000000) + after.tv_nsec;

  //tempo decorrido em segundos
  double elapsedT = (after_ns - before_ns)*pow(10,-9);

  //imprime o tamanho do ficheiro e o tempo que demorou a ser transferido
  printf("File size: %d bytes\nTransfer time: %.4f sec\n", tamanho_int,elapsedT);

  //fecha o ficheiro escrito
  fclose(file);  

  //fecha a ligação com a porta série 
  if(llclose(fd) == -1)
    exit(-1);

  return 0;
}
