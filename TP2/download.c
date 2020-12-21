#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clientTCP.c"


/**
 * retira os argumentos do url contidos em arg
 * @param arg string que representa o url 
 * @return retorna uma struct urlInfo com a informação lida de arg
 */ 
struct urlInfo readUrlFromArgv(char * arg){

    struct urlInfo ret; // vai guardar temporariamente a informação do url para depois ser retornada

    // verificar se a parte inicial do url está correta
    if ( strncmp("ftp://",arg,6) != 0){
        printf("Error: invalid url format try ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(-2);
    }

    char * rest = &arg[6]; // retira de arg o que já foi analisado

    char * userAndPassMidle = strchr(rest,':'); // apontador para o meio do username e password
    char * userAndPassEnd = strchr(rest,'@'); // apontador para o fim da parte onde se encontra o username e a password

    if ( (userAndPassEnd==NULL) && (userAndPassMidle == NULL) ){ // se estiverem em falta os dois limitadores do username e da password
        strcpy(ret.username,"anonymous"); // se não tem user nem pass então usar anonymous
        strcpy(ret.password,"rcom"); // pode ser uma password qualquer
    } else if ( (userAndPassEnd!=NULL) && (userAndPassMidle == NULL)){ // se estiver em falta o limitador do username 
        printf("Error: invalid url username and password try ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(-3);
    } else if ( (userAndPassEnd==NULL) && (userAndPassMidle != NULL)){ // se estiver em falta o limitador da password 
        int lenUser = (userAndPassMidle-rest); // calcular o tamanho do username
        strncpy(ret.username,rest,lenUser); // atribuir o username
        strcpy(ret.password,""); // atribuir a password

        rest = (userAndPassMidle+1); // colocar no rest o que falta analisar
    }else{
        int lenUser = (userAndPassMidle-rest); // calcular o tamanho do username
        int lenPass = (userAndPassEnd - userAndPassMidle) -1; // calcular o tamanho da password

        strncpy(ret.username,rest,lenUser); // atribuir o username
        strncpy(ret.password,userAndPassMidle+1,lenPass); // atribuir a password

        rest = (userAndPassEnd+1); // colocar no rest o que falta analisar
    }

    char * primeiraBarra = strchr(rest,'/'); // apontador para o fim do host e o inicio do path
    
    if ( primeiraBarra == NULL){ // verifica se não existe nenhuma barra a dividir o host do path
        printf("Error: invalid url format try ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(-4);
    }
    
    if( primeiraBarra == rest){ // verifica se o host é vazio
        printf("Error: invalid url host try ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(-5);
    }
    
    int lenHost = primeiraBarra - rest; // calcular o tamanho do host
    
    strncpy(ret.host,rest,lenHost); // atribuir o host
    
    rest = primeiraBarra+1; // colocar no rest o que falta analisar

    strcpy(ret.path,rest); // atribuir o path

    if(strchr(rest,'/') ==NULL){ // se o ficheiro estiver na origem
        strcpy(ret.filename,ret.path); // atribuir o filename
        strcpy(ret.path,""); // limpar o path pois não é necessário
    }else{
        char * path = ret.path; 
        char * ultimaBarra;
        while (1) // enquanto não chegar ao diretório que tem o ficheiro
        {
            ultimaBarra = strchr(path,'/');
            if (ultimaBarra == NULL)
                break;

            path = ultimaBarra+1;
        }
        strcpy(ret.filename,path); // atribuir o filename 
        strcpy(path,""); // apagar o filename que está no final do path
    }

    return ret;
}


int main(int argc,char*argv[]){

    if (argc != 2){ // verifica se foram introduzidos na linha de comandos um numero de argumentos diferente de 2
        printf("Error: invalid number of arguments, try ./download <url>\n");
        exit(-1);
    }

    struct urlInfo readedUrl = readUrlFromArgv(argv[1]);

    /*
    printf("%s\n",readedUrl.username);
    printf("%s\n",readedUrl.password);
    printf("%s\n",readedUrl.host);
    printf("%li\n",strlen(readedUrl.host));
    printf("%s\n",readedUrl.path);
    printf("%s\n",readedUrl.filename);
    printf("%li\n",strlen(readedUrl.filename));*/
    
    return downloadFileFromServer(readedUrl);
}