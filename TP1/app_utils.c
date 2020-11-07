#include <sys/types.h>
#include <termio.h>
#define NUM_BAUND_RATES 19

struct baud_rate{
  speed_t baud;
  char *bauds;
};

//struct que vai guardar os baudrates para melhor acessibilidade 
struct baud_rate rates[NUM_BAUND_RATES] = {
  {B0,"B0"},
  {B50,"B50"},
  {B75,"B75"},
  {B110,"B110"},
  {B134,"B134"},
  {B150,"B150"},
  {B200,"B200"},
  {B300,"B300"},
  {B600,"B600"},
  {B1200,"B1200"},
  {B1800,"B1800"},
  {B2400,"B2400"},
  {B4800,"B4800"},
  {B9600,"B9600"},
  {B19200,"B19200"},
  {B38400,"B38400"},
  {B57600,"B57600"},
  {B115200,"B115200"},
  {B230400,"B230400"},
};

/**
 * vai procurar na struct de baudrate se o baudrate inserido existe
 * @param rate baudrate em string 
 * @return baudrate em speed_t
 **/ 
speed_t getBaudRate(char * rate){
    for (size_t i = 0; i < NUM_BAUND_RATES; i++)
    {
        if (strcmp(rates[i].bauds,rate) == 0){
            return rates[i].baud;
        }
    }
    printf("Error: Incorrect BaudRate!\n");
    return -1;
}