# Instruções

## Emisor

#### Compilar
$ make app

#### Recompilar
$ make again

#### Correr Executavel
O emisor tem de ser executado com 3 argumentos (incluindo ./app), e a ordem tem de ser respeitada.<br>
O tamanho da trama e o baudrate são opcionais, e se não forem colocados eles assumem um valor default pré-definido.<br>
(frame_size = 1000 e baudrate = 38400)<br>
A ordem do tamanho da trama e do baudrate não presica de ser respeitada mas para cada um dos argumentos é necessário colocar um prefixo para que o programa não dê erro.<br>
Prefixos:
- "--frame-size" para o tamanho da trama;
- "-B" para o baudrate.

$ ./app /dev/ttySx file_name [ --frame-size 1000 -B 38400]

#### Remover o executavel
$ make clear

## Receptor

#### Compilar
$ make app

#### Recompilar
$ make again

#### Correr Executavel
O receptor tem de ser executado com 2 argumentos (incluindo ./app), e a ordem tem de ser respeitada.<br>
O baudrate é opcional, e se não for colocado ele assume um valor default pré-definido. (baudrate = 38400)<br>
Prefixos:
- "-B" para o baudrate.

$ ./app /dev/ttySx [-B 38400]

#### Remover o executavel
$ make clear
