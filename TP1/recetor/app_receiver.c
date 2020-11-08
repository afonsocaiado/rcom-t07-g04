#include "protocol_receiver.c"
#include "../app_utils.c"

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

int main(int argc, char** argv)
{
  int fd,porta; // descritor da ligacao de dados
  unsigned char mensagemPronta[65540]; // variavel onde cada pacote vai ser guardado
  int sizeMessage = 0; // tamanho do pacote
  int sizeOfStart = 0; // tamanho do pacote START
  unsigned char start[100]; // variavel que guarda o pacote START
  time_t incial,final; // inicial vai guardar o tempo de quando começa a receber informação, e final vai guardar o tempo de quando acabar de transferir a informação
  struct shell_inputs arguments;
  
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
  porta = atoi(&arguments.port[9]); // numero da porta de comunicação tty

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
  int tamanho_int = atoi(tamanho_str); //converter o tamanho em string para inteiro

  //recolher a informação do nome do ficheiro
  int num_blocos_nome = start[3+num_blocos_tamanho+1];
  char nome_ficheiro[num_blocos_nome+1];
  for (size_t i = 0; i < num_blocos_nome; i++)
  {
    nome_ficheiro[i] = start[5 + num_blocos_tamanho + i];
  }
  nome_ficheiro[num_blocos_nome]= '\0';

  // cria ficheiro com os dados das tramas de informacao recebidas
  FILE *file = fopen(nome_ficheiro, "wb+");

  incial = time(NULL);//guarda o tempo atual em incial

  //enquanto houver informação para ler
  while (TRUE)
  {
    sizeMessage = llread(fd, mensagemPronta); //lê de fd um pacote de informação 

    if(sizeMessage == -1)
      exit(-1);
    if (sizeMessage == 0)
      continue;

    if (isAtEnd(start, sizeOfStart, mensagemPronta, sizeMessage)) // se leu a trama de END
    {
      final = time(NULL); //guarda o tempo atual em final
      printf("End message received\n");
      break;
    }

    int sizeWithoutHeader = 0; //guarda o tamanho do pacote de dados sem o cabeçalho 
    sizeWithoutHeader = sizeMessage - 4;

    // remove o cabeçalho do nível de aplicação das tramas de informacao
    int i = 0;
    int j = 4;
    unsigned char messageRemovedHeader[sizeWithoutHeader]; //array que vai guardar os dados recebidos
    for (; i < sizeMessage; i++, j++)
    {
      messageRemovedHeader[i] = mensagemPronta[j];
    }
    
    //escreve no ficheiro file os dados recebidos 
    fwrite(messageRemovedHeader, 1, sizeWithoutHeader, file);
  }

  printf("File size: %d bytes\nTransfer time: %.2f sec\n", tamanho_int,difftime(final,incial));
  
  //fecha o ficheiro escrito
  fclose(file);


  //fecha a ligação com a porta série 
  if(llclose(fd) == -1)
    exit(-1);

  return 0;
}
