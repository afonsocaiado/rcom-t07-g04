# Instruções

## Emissor

#### Compilar
$ make app

#### Recompilar
$ make again

#### Correr Executavel
O emissor tem de ser executado com 3 argumentos (incluindo ./app), e a ordem tem de ser respeitada.<br>
O tamanho da trama e o baudrate são opcionais, e se não forem colocados eles assumem um valor default pré-definido.<br>
(frame_size = 1000 e baudrate = 38400)<br>
A ordem do tamanho da trama e do baudrate não presica de ser respeitada mas para cada um dos argumentos é necessário colocar um prefixo para que o programa não dê erro.<br>
Prefixos:
- "--frame-size" para o tamanho da trama;
- "-B" para o baudrate.

$ ./app /dev/ttySx file_name [ --frame-size 1000 -B 38400]

#### Remover o executavel
$ make clean

## Receptor

#### Compilar
$ make app

#### Recompilar
$ make again

#### Correr Executavel
O receptor tem de ser executado com 2 argumentos (incluindo ./app), e a ordem tem de ser respeitada.<br>
O baudrate, o FER e o delay são opcional, e se não forem colocados eles assumem um valor default pré-definido. (baudrate = 38400 , FER= 0% e D=0 )<br>
O valor de FER varia entre 0 e 100 e indica a probabilidade de forçar erros nas tramas. <br>
O valor do delay tanto pode ser 0 ou 1, e indica se o delay está ligado ou desligado.<br>
Prefixos:
- "-B" para o baudrate;
- "-FER" para forçar erros nas tramas I;
- "-D" para adicionar atraso no processamento da trama I.

$ ./app /dev/ttySx [-B 38400 -FER 0 -D 0]

#### Remover o executavel
$ make clean
