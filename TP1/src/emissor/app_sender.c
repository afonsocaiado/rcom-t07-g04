#include "protocol_sender.c"
#include "../app_utils.c"

#define DADOS 1
#define START 2
#define END 3

//indentificadores de informação 
enum T { TAMANHO = 0 , NOME = 1};

int main(int argc, char** argv)
{
    int fd, porta , num_sequencia = 0; // (fd) - descritor da ligacao de dados, (porta) - porta serie, (num_sequencia) - numero de sequencia do pacote de dados
    int frame_size; //tamanho máximo em bytes dos dados contidos no pacote de dados
    unsigned int tamanho; //tamanho do ficheiro em blocos de 1 byte
    FILE * ficheiro; // ficheiro que irá ser enviado
    char * nome_ficheiro; // nome do fiheiro a ser enviado
    int tamanho_dados = 4; //tamanho do pacote de dados inicialmente
    int tamanho_controlo = 5; //tamanho do pacote de controlo inicialmente
    char * controlo = (char *) malloc(tamanho_controlo); // guarda a informação que irá conter um pacote de controlo
    char * dados = (char * )malloc(tamanho_dados); // guarda a informação que irá conter um pacote de dados
    struct shell_inputs arguments; // struct que vai guardar os argumentos introduzidos
    
    //verifica se foi introduzido pelo menos 3 argumentos, e se segundo argumento começa por /dev/ttyS
    if ( (argc < 3) || (strncmp("/dev/ttyS",argv[1],9) != 0) ) {
    printf("Invalid inputs, the template command is:\n./app /dev/ttySx file_name (--frame-size 1000 -B 38400)\n");
    exit(1);
    }

    //vai buscar ao array argv os argumentos introduzidos 
    if (readArgvValues(argc,argv,&arguments,TRANSMITTER) == -1){
        exit(-1);
    }
    
    //atribuição dos argumentos lidos
    frame_size = arguments.frame_size;
    if (frame_size > 65535){ // caso o frame_size exceda o tamanho máximo
        printf("Frame size exceeded!\n");
        exit(-1);
    }
    nome_ficheiro = arguments.file_name; 
    BAUDRATE = arguments.baudrate;
    porta = atoi(&arguments.port[9]); 
    
    fd = llopen(porta);

    // verificar se a porta serie foi aberta com sucesso
    if (fd == -1){
        free(controlo);
        free(dados);
        exit(-1);
    }

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

        //envia o pacote de controlo START ao receptor
        int bytesEscritos = llwrite(fd, controlo,tamanho_controlo);

        if ( bytesEscritos < 0) // se ocorreu um erro no llwrite
            exit(-1);
        
        //envio de dados
        char buffer[frame_size];
        int bytesLidos = fread(buffer,1,frame_size,ficheiro);
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
            
            //envia o pacote de dados para o receptor
            bytesEscritos = llwrite(fd,dados,tamanho_dados+bytesLidos);

            if ( bytesEscritos < 0){ // se ocorreu um erro no llwrite
                free(controlo);
                free(dados);
                exit(-1);
            }

            if(num_sequencia == 255) // caso o numero de sequencia já tenha chegado ao máximo
                num_sequencia = 0;
            else
                num_sequencia++; //incrementar o numero de sequência
            
            bytesLidos = fread(buffer,1,frame_size,ficheiro);          
        } 
        
        //contrução do pacote de controlo END
        controlo[0] = END;

        //envia o pacote de controlo END para o receptor
        bytesEscritos = llwrite(fd, controlo,tamanho_controlo);

        if ( bytesEscritos < 0) // se ocorreu um erro no llwrite
            exit(-1);
        

    }else{ // caso do ficheiro indicado pelo utilizador não existir
        printf("O ficheiro %s não existe.\n",nome_ficheiro);
        free(controlo);
        free(dados);
        llclose(fd);
        exit(-1);
    }

    //fecha o ficheiro lido
    fclose(ficheiro);

    //verificar se a porta serie foi fechada com sucesso
    if (llclose(fd) == -1){
        free(controlo);
        free(dados);
        exit(-1);
    }

    //liberta a memoria utilizada
    free(controlo);
    free(dados);
    
    return 0;
}