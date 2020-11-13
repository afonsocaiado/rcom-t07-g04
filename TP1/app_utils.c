#include <sys/types.h>
#include <termio.h>
#define NUM_BAUND_RATES 19
#define TRANSMITTER 1
#define RECEIVER 0
#define DEFAULT_BAUDRATE B38400
#define DEFAULT_FRAME_SIZE 1000

//struct que define um baudrate
struct baud_rate{
  speed_t baud; //baudrate em speed_t
  char *bauds; //baudrate em string
};

//struct que vai guardar os dados necessários para o calculo da eficiência
struct dataForEfficiency
{
  double Tprop[256]; //array que contem os tempos de propagação
  int arraySize; //tamanho do array Tprop e do array frame_size
  int frame_size[256]; //tamanho da trama
  char baudrate[10]; //baudrate utilizado
  double FER; //frame error ratio
};


//guarda os parametros passados pela shell
struct shell_inputs
{
  char port[15]; //Dispositivo /dev/ttySx, x= 0,1,10,11 
  char file_name[200]; //nome do ficheiro a enviar
  int frame_size; //tamanho maximo da quantidade de dados que devem ser enviados de uma vez
  speed_t baudrate; // baudrate da ligação serie
  char baud[10]; //baudrate em string
};

//struct que vai guardar os baudrates para melhor acessibilidade 
struct baud_rate rates[NUM_BAUND_RATES] = {
  {B0,"0"},
  {B50,"50"},
  {B75,"75"},
  {B110,"110"},
  {B134,"134"},
  {B150,"150"},
  {B200,"200"},
  {B300,"300"},
  {B600,"600"},
  {B1200,"1200"},
  {B1800,"1800"},
  {B2400,"2400"},
  {B4800,"4800"},
  {B9600,"9600"},
  {B19200,"19200"},
  {B38400,"38400"},
  {B57600,"57600"},
  {B115200,"115200"},
  {B230400,"230400"},
};

/**
 * vai procurar na struct de baudrates se o baudrate inserido existe
 * @param rate baudrate em string 
 * @return baudrate em speed_t ou -1 em caso de erro
 **/ 
speed_t getBaudRate(char * rate){
    for (size_t i = 0; i < NUM_BAUND_RATES; i++)
    {
        if (strcmp(rates[i].bauds,rate) == 0){
            return rates[i].baud;
        }
    }
    return -1;
}

/**
 * função responsavel por ler os argumentos introduzidos na linha de comandos
 * @param argc tamanho do array argv
 * @param argv array que contém os argumentos introduzidos
 * @param arguments struct que vai guardar os valores dos argumentos introduzidos
 * @param status flag que indica se é o emisor ou o receptor
 * @return 0 em caso de sucesso e -1 caso contrário 
 **/
int readArgvValues(int argc ,char *argv[],struct shell_inputs *arguments, int status){
  strcpy(arguments->port,argv[1]);
  if(status){ // se for o emisor 
    strcpy(arguments->file_name,argv[2]);

    //coloca os valores default de baudrate e frame size
    arguments->baudrate = DEFAULT_BAUDRATE;
    arguments->frame_size = DEFAULT_FRAME_SIZE;

    //percorre os restantes valores de argv
    for (size_t i = 3; i < argc; i++){
      if ( (strcmp(argv[i],"--frame-size") == 0 ) && ((i+1)<argc) ){ // lê o valor da frame size introduzido
        arguments->frame_size = atoi(argv[i+1]);
        if (arguments->frame_size <= 0){ // verifica se o valor de frame size é válido 
          printf("Invalid Frame Size, the template command is:\n./app /dev/ttySx file_name [--frame-size 1000 -B 38400]\n");
          return -1;
        }
        i++;
      }else if ( (strcmp(argv[i],"-B") == 0) && ((i+1)<argc) ){ // lê o valor da baudrate introduzido 
        arguments->baudrate = getBaudRate(argv[i+1]);
        if (arguments->baudrate == -1){ // se o valor de baudrate estiver errado
          printf("Invalid Baudrate, the template command is:\n./app /dev/ttySx file_name [--frame-size 1000 -B 38400]\n");
          return -1;
        }
        i++;
      }else{ // se existir um argumento invalido
        printf("Invalid inputs, the template command is:\n./app /dev/ttySx file_name [--frame-size 1000 -B 38400]\n");
        return -1;
      }     
    }
  }else{ // se for o receptor
    //coloca o valor default do baudrate
    arguments->baudrate = DEFAULT_BAUDRATE;
    strcpy(arguments->baud,"38400");

    //percorre os restantes valores de argv
    for (size_t i = 2; i < argc; i++)
    {
      if ( (strcmp(argv[i],"-B") == 0) && ((i+1)<argc) ){ // lê o valor da baudrate introduzido 
        arguments->baudrate = getBaudRate(argv[i+1]);
        strcpy(arguments->baud , argv[i+1]);
        if (arguments->baudrate == -1){ // se o valor de baudrate estiver errado
          printf("Invalid Baudrate, the template command is:\n./app /dev/ttySx [-B 38400]\n");
          return -1;
        }
        i++;
      }else{ // se existir um argumento invalido
        printf("Invalid inputs, the template command is:\n./app /dev/ttySx [-B 38400]\n");
        return -1;
      }    
    }
  }

  return 0;
}