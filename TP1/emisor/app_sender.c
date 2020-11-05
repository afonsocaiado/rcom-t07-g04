#include "writenoncanonical.c"

#define DADOS 1
#define START 2
#define END 3

enum T { TAMANHO = 0 , NOME = 1};

int trama_size = 10;  //tamanho default de envio da trama é 10 bytes

int main(int argc, char** argv)
{
    int fd, porta , num_sequencia = 0;
    unsigned int tamanho; //tamanho do ficheiro em blocos de 1 byte
    FILE * ficheiro; // ficheiro que irá ser enviado
    char * nome_ficheiro = argv[2]; // nome do fiheiro a ser enviado
    int tamanho_dados = 4; //tamanho do pacote de dados inicialmente
    int tamanho_controlo = 5; //tamanho do pacote de controlo inicialmente
    char * controlo = (char *) malloc(tamanho_controlo);
    char * dados = (char * )malloc(tamanho_dados);
    /*
    if ( (argc < 2) || 
        ((strcmp("/dev/ttyS0", argv[1])!=0) && 
        (strcmp("/dev/ttyS1", argv[1])!=0) )) {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
    exit(1);
    }*/

    if(argc == 4){
        trama_size = atoi(argv[3]);
        if (trama_size > 64000){
            printf("Frame size exceeded!\n");
            exit(-1);
        }
    }

    porta = atoi(&argv[1][9]); // numero da porta de comunicação tty
    
    fd = llopen(porta);

    // verificar se a porta serie foi aberta com sucesso
    if (fd == -1)
        exit(-1);

    //abre o ficheiro para leitura 
    ficheiro = fopen(nome_ficheiro,"r");

    //verificar se o ficheiro foi aberto com sucesso
    if (ficheiro !=NULL){
        
        //calcular o tamanho do ficheiro
        fseek(ficheiro,0,SEEK_END);
        tamanho = ftell(ficheiro);

        rewind(ficheiro); //voltar ao inicio do ficheiro

        //construção do pacote de controlo de START
        controlo[0] = START;
        controlo[1] = TAMANHO;
        
        //passar o tamanho do ficheiro para string e dps adicionar ao array controlo
        char size[10];
        sprintf(size,"%d",tamanho);
        int num_blocos_tamanho = strlen(size); //guarda o numero de blocos de 1 byte que são necessãrios para guardar a informação do tamanho do ficheiro
        controlo[2] = num_blocos_tamanho;
        tamanho_controlo += num_blocos_tamanho;
        
        controlo = (char * )realloc(controlo,tamanho_controlo); //alocar mais memoria para ser possivel guardar os blocos que correspondem ao tamanho do ficheiro

        memcpy(controlo+3,size,num_blocos_tamanho);
        
        //adicionar ao pacote de controlo a informação do nome do ficheiro
        controlo[3+num_blocos_tamanho] = NOME;
        controlo[4+num_blocos_tamanho] = strlen(nome_ficheiro);
        
        tamanho_controlo += strlen(nome_ficheiro);
        controlo= (char * ) realloc (controlo,tamanho_controlo);//necessário alocar mais memória para guardar o nome do ficheiro
        memcpy(controlo+5+num_blocos_tamanho,nome_ficheiro,strlen(nome_ficheiro));

        
        int bytesEscritos = llwrite(fd, controlo,tamanho_controlo);

        if ( bytesEscritos < 0) // se ocorreu um erro no llwrite
            exit(-1);
        
        //envio de dados

        char buffer[trama_size];
        int bytesLidos = fread(buffer,1,trama_size,ficheiro);
        dados[0] = DADOS;
        while (bytesLidos > 0){ //enquanto houver dados para enviar
            //construção do pacote de dados
            dados[1] = num_sequencia;
            dados[2] = bytesLidos/255;
            dados[3] = bytesLidos%255;

            dados = (char * )realloc(dados,tamanho_dados + bytesLidos); //adiciona mais espaço ao array para poder guardar a informação do ficheiro

            for (size_t i = 0; i < bytesLidos; i++)
            {
                dados[4+i] = buffer[i];
            }
            
            bytesEscritos = llwrite(fd,dados,tamanho_dados+bytesLidos);

            if ( bytesEscritos < 0) // se ocorreu um erro no llwrite
                exit(-1);
            
            num_sequencia++; //incrementar o numero de sequência
            
            bytesLidos = fread(buffer,1,trama_size,ficheiro);
                        
        } 
        
        //contrução do pacote de controlo END
        controlo[0] = END;

        bytesEscritos = llwrite(fd, controlo,tamanho_controlo);

        if ( bytesEscritos < 0) // se ocorreu um erro no llwrite
            exit(-1);
        

    }else{
        printf("O ficheiro %s não existe.\n",nome_ficheiro);
    }

    fclose(ficheiro);

    //verificar se a porta serie foi fechada com sucesso
    if (llclose(fd) == -1)
        exit(-1);

    free(controlo);
    free(dados);
  return 0;
}