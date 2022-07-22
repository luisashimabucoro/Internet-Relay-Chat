#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <thread>
#include <atomic>
#include <arpa/inet.h>
#include <iostream>

using namespace std;

#define TAMANHO_USUARIO 50 
#define TAMANHO_BUFFER 4096 
#define TAMANHO_SERVIDOR 8192 
#define TAMANHO_MAXIMO_BUFFER 40960 

char nomeUsuario[TAMANHO_USUARIO];
char buffer[TAMANHO_BUFFER]; 
char servidorBuffer[TAMANHO_SERVIDOR]; 
char bufferMaximo[TAMANHO_MAXIMO_BUFFER]; 
char mensagem[TAMANHO_BUFFER+TAMANHO_USUARIO+2]; 
int socketUsuario, numeroPorta, n;
std::atomic<bool> desconectado (false); 

// Função para enviar uma mensagem de erro, casa ocorra
void erro(const char *msg){
    // A função perror mapeia o erro numérico para uma mensagem de erro
    perror(msg);
    exit(1);
}


void gravar_nome_usuario(){

    // Inicializando o array que receberá o nome do usuário
    char array_nome_usuario[256];

    printf("Nome: ");
    fgets(array_nome_usuario, 256, stdin);

    if(strlen(array_nome_usuario) > 50){
        printf("Nome grande demais, iremos considerar apenas as primeiras 20 letras\n");
        array_nome_usuario[TAMANHO_USUARIO] = '\0';
    } else {
        array_nome_usuario[strlen(array_nome_usuario)-1] = '\0';
    }
    
    // Armazenamos o nome informado pelo usuário na variável que grava seu nome
    strcpy(nomeUsuario, array_nome_usuario);
    array_nome_usuario[0] = '\0';
}


void gerencia_envio(){

     // Enquanto o usuário estiver conectado
    while(!desconectado){

        // Limpa os buffers de mensagem para armazenarem a mensagem digitada
        bzero(bufferMaximo, TAMANHO_MAXIMO_BUFFER); 

        bzero(mensagem, TAMANHO_BUFFER+TAMANHO_USUARIO+2);

        // Leitura da entrada para armazenamento da mensagem
        fgets(bufferMaximo, TAMANHO_MAXIMO_BUFFER, stdin);
        bufferMaximo[strlen(bufferMaximo)-1] = '\0';
        
        // Checa se a mensagem está ultrapassando o tamanho máximo permitido (4096)
        if(strlen(bufferMaximo) > TAMANHO_BUFFER){
            int tamanhoBuffer = strlen(bufferMaximo);
            int aux = 0;

            // Divide a mensagem até que o tamanho seja menor que o tamanho máximo permitido
            while(tamanhoBuffer > TAMANHO_BUFFER){
                
                // Copia parte da mensagem para o buffer "original", dividindo a mensagem completa
                strncpy(buffer, bufferMaximo+aux, TAMANHO_BUFFER);
                buffer[TAMANHO_BUFFER] = '\0';

                // A partir da 2ª parte, as mensagens terão uma linebreak as antecedendo
                // para separá-las visualmente das partes anteriores
                if (aux == 0){
                    // Armazena a mensagem dividida em "mensagem"
                    sprintf(mensagem, "%s: %s\n", nomeUsuario, buffer);
                } else {
                    // Armazena a mensagem dividida em "mensagem"
                    sprintf(mensagem, "\n%s: %s\n", nomeUsuario, buffer);
                }

                /*
                A função write() irá enviar a mensagem para o servidor utilizando
                o socketUsuario e retornará o número de bytes enviados
                Caso o número de bytes enviados seja diferente do tamanho da mensagem,
                significa que ocorreu um erro
                'n' irá armazenar o retorno, para saber se o envio foi bem sucedido
                */

                n = write(socketUsuario, mensagem, strlen(mensagem));   
                
                // Caso o envio não seja bem sucedido, uma mensagem de erro é exibida
                if (n == -1) {
                    erro("Falha no envio");
                }

                // Limpa os buffers de mensagem e a mensagem em si
                bzero(buffer, TAMANHO_BUFFER); 
                bzero(mensagem, TAMANHO_BUFFER+TAMANHO_USUARIO+2);

                // Como parte da mensagem foi transferida para outro buffer
                // Decrementamos o tamanho dessa parte do tamanho total da mensagem
                tamanhoBuffer -= TAMANHO_BUFFER;
                aux += TAMANHO_BUFFER;
            }
            // Caso o tamanho da mensagem tenha se tornado menor que o tamanho máximo permitido
            // A mensagem é enviada para o buffer original
            strncpy(buffer, bufferMaximo+aux, tamanhoBuffer);
        } else {
            // Caso o tamanho da mensagem original já seja menor ou igual ao tamanho máximo permitido
            // A mensagem é enviada para o buffer original
            strcpy(buffer, bufferMaximo);
        }
        // Formata a mensagem com o nome do usuário seguido de sua mensagem
        sprintf(mensagem, "%s: %s", nomeUsuario, buffer);

        // Envia a mensagem para o servidor
        // 'n' irá armazenar o retorno, para saber se o envio foi bem sucedido
        n = write(socketUsuario, mensagem, strlen(mensagem));   

        // Caso o envio não seja bem sucedido, uma mensagem de erro é exibida
        if (n == -1){
            erro("Falha no envio da mensagem");
        } 
    }
}

void gerencia_mensagens_recebidas(){
    // Enquanto o usuário estiver conectado
    while(!desconectado){

        // Limpa o buffer de mensagem do servidor mais o tamanho do nome do servidor
        bzero(servidorBuffer, TAMANHO_SERVIDOR + 10); 

        // Recebe a mensagem do servidor
        // 'n' irá armazenar o retorno, para saber se o recebimento foi bem sucedido
        n = read(socketUsuario, servidorBuffer, TAMANHO_SERVIDOR + 10); 

        // Caso o recebimento não seja bem sucedido, uma mensagem de erro é exibida
        if (n == -1) {
            erro("Erro na leitura de socket");
        }

        // Caso o servidor se desconectar, o usuário é desconectado junto
        else if (n == 0) { 
            printf("Servidor offline");
            desconectado = true;
        }

        // Caso o servidor enviar uma mensagem válida, ela é exibida
        else if(n > 1){
            printf("%s\n",servidorBuffer); 
        }
    }
}

int main(int argc, char *argv[]){
    
    // sockaddr_in é uma struct para gerenciar endereços de internet
    struct sockaddr_in enderecoServidor;
    if (argc < 2) { 
        erro("Erro! Por favor informe a porta no Makefile!\n");
    }

    // Converte para inteiro a porta recebida por argumento
    numeroPorta = atoi(argv[1]);
    
    /*
    Cria um socket que será utilizado pelo usuário
    'AF_INET' é o domínio de comunicação utilizado para comunicação IPv4
    'SOCK_STREAM' é o tipo do socket
    '0' é o valor que indica o protocolo que será utilizado pelo socket
    */
    socketUsuario = socket(AF_INET, SOCK_STREAM, 0); 

    // Caso a criação do socket retorne -1, significa que houve um erro
    if (socketUsuario == -1){
        erro("Erro ao criar Socket\n");
    }

    // servidorIP irá armazenar o IP do servidor cujo usuário deseja conectar
    string servidorIP;
    printf("Informe o IP do servidor: ");
    getline(cin, servidorIP);

    
    // Inicializa a struct enderecoServidor com 0s
    bzero( (char *) &enderecoServidor, sizeof(enderecoServidor));   

    enderecoServidor.sin_family = AF_INET;
    // Converto o IP do servidor para um formato de endereço de internet e o armazeno
    enderecoServidor.sin_addr.s_addr = inet_addr(servidorIP.c_str());	
    // 'htons' converte um valor de porta de host para o formato de porta de internet
    enderecoServidor.sin_port = htons(numeroPorta);
    
    /*
    Se a conexão for bem sucedida, retorna 0 e o funcionamento prossegue
    caso contrário, retorna -1 e exibe uma mensagem de erro
    */
    if (connect(socketUsuario, (struct sockaddr *) &enderecoServidor, sizeof(enderecoServidor)) == -1){
        erro("Erro na conexão do Usuário\n");
    }

    /*
    Caso a conexão seja bem sucedida, é feita a leitura do socket e armazenará em um buffer
    A função read() irã realizar a leitura e armazenar no 'buffer', 4095 é o tamanho de bytes a serem lidos
    */
    n = read(socketUsuario, buffer, 4095);

    // Caso a leitura não seja bem sucedida, uma mensagem de erro é exibida
    if (n == -1){
        erro("Erro na leitura de socket");
    }
    
    printf("\n%s", buffer);
    printf("  ╔═════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("  ║Bem vindo(a) ao chat, escolha seu nome de usuário: (Limite de 20 caracteres)     ║\n");
    printf("  ╚═════════════════════════════════════════════════════════════════════════════════╝\n\n");

    
    gravar_nome_usuario();
    write(socketUsuario, nomeUsuario, TAMANHO_USUARIO);    

    // Cria um thread para gerenciar as mensagens enviadas pelo usuário
    // Cria uma thread para gerenciar as mensagens recebidas pelo usuário
    thread threadEnviaMensagens (gerencia_envio);
    // Separa a thread de execução para continuar sendo executada independentemente.
    threadEnviaMensagens.detach();
    thread threadRecebeMensagens (gerencia_mensagens_recebidas);
    // Separa a thread de execução para continuar sendo executada independentemente.
    threadRecebeMensagens.detach();

    while(!desconectado){
    // Enquanto o usuário estiver conectado, o cliente permancerá no servidor
    // E o código continuará sendo rodado
    }

    // Caso o usuário tenha sido desconectado por algum motivo, fecha o socket e desliga o usuário
    close(socketUsuario);
    return 0;
}
