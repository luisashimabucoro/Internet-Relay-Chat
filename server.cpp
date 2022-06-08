// Bibliotecas padrões:
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

// Biblioteca para utilizar bool e threads
#include <thread>
#include <atomic>

/*
Tamanho do buffer está 2048 e o máximo como 4096
Isso será utilizado para caso uma mensagem ultrapasse o limite
máximo de 4096, ela será dividida entre os buffers suscetivamente
até que o tamanho da mensagem seja menor que 4096.
*/
#define TAMANHO_BUFFER 4096 
#define TAMANHO_MAXIMO 40960     

// Limitamos o tamanho do nome do usuário, até para não ficar uma interface ruim no chat
#define TAMANHO_USUARIO 10 

// Inicialmente, a quantidade de usuários será igual a 0
int qtdUsuarios = 0;

// 
char buffer[TAMANHO_BUFFER];  
char bufferMax[TAMANHO_MAXIMO+10]; 
char message[TAMANHO_BUFFER+TAMANHO_USUARIO+2];
bool flagFull = true; 
int serverSocket; 
static bool usuarioDesconectado = false; 
std::atomic<bool> serverOFF (false);

typedef struct usuario {
    int socketID;
    struct sockaddr_in endereco;
    char nome[TAMANHO_USUARIO];
    int clientID;
    bool conectado;
}USUARIO;

USUARIO clients[2]; // stores every usuario conectado to Servidor


// Função para enviar uma mensagem de erro, casa ocorra
void erro(const char *msg){
    // A função perror mapeia o erro numérico para uma mensagem de erro
    perror(msg);
    exit(1);
}


void checa_saida(char message[]){

    // Caso o comando seja /quit, o servidor é desligado
    // e os sockets são fechados
    if (strcmp(message, "\\quit") == 0){    
        serverOFF = true;

        // Fechar os sockets
        for (int i = 0; i < qtdUsuarios; i++){
            close(clients[i].socketID);
        }
        close(serverSocket);
        exit(0);
    }
}

// Função para enviar a mensagem aos usuarios
void envia_mensagem(char* message, int userID, bool sendAll) {
    // Caso a mensagem seja maior que o tamanho máximo, ela será dividida
    if(strlen(message) > TAMANHO_MAXIMO){
        write(clients[0].socketID, "grande", 6);      
    }
    for(int i = 0; i < 2; i++){
        if((clients[i].clientID != userID && clients[i].conectado == true) || (sendAll==true && clients[i].conectado == true)){
            if (write(clients[i].socketID, message, strlen(message)) < 0 && usuarioDesconectado == false) {
                erro("\nErro!\n");
            }
        }
    }

}

void configurar_cliente(USUARIO usuario){
    
    // Inicializa informações do cliente
    char nome_usuario[TAMANHO_USUARIO];
    usuarioDesconectado = false;
    qtdUsuarios++;

    // Recebe o nome do usuário e verifica se é válido
    if(recv(usuario.socketID, nome_usuario, TAMANHO_USUARIO, 0) <= 0){
        erro("\nErro ao receber nome!\n");
        usuarioDesconectado = true;
    }
    else{
        // Se o nome for válido, grava ele no nome do usuario
        strcpy(usuario.nome, nome_usuario);
    }
    
    // Limpa o buffer de mensagem
    bzero(message, TAMANHO_BUFFER+TAMANHO_USUARIO+2);

    // Enquanto o usuário estiver no servidor 
    while(!usuarioDesconectado){

        // Recebe os dados do socket 
        int dadoDoSocket = recv(usuario.socketID, message, TAMANHO_BUFFER+TAMANHO_USUARIO+2, 0);

        // Caso receba mais de 0 bytes, checa o tamanho da mensagem e a envia para o servidor
        if(dadoDoSocket > 0){
            if (strlen(message) > 0) {
                envia_mensagem(message, usuario.clientID, true);
                printf("%s\n", message); 
            }
        }
        // Caso o socket retorne 0, significa que a conexão com o usuário foi encerrada
        else if (dadoDoSocket == 0 || strcasecmp("quit\n", message) == 0) {
            sprintf(message, "\n %s Desconectou \n\n", usuario.nome);
            printf("%s", message);

            envia_mensagem(message, usuario.clientID, false);
            usuarioDesconectado = true;
        } else {
            erro("Erro, usuário desconectado\n");
            usuarioDesconectado = true; 
        }
        bzero(message, TAMANHO_BUFFER+TAMANHO_USUARIO+2);
    }

    // Remove o usuário do servidor
    usuario.conectado = false;
    close(usuario.socketID);
    qtdUsuarios--;
    flagFull = false;
}

void gerencia_envio(){

    // Enquanto o servidor estiver rodando
    while(!serverOFF){

        // Limpa os buffers 
        bzero(bufferMax, TAMANHO_MAXIMO); 
        bzero(message,TAMANHO_BUFFER+TAMANHO_USUARIO+2); 

        // Lê a mensagem de entrada 
        fgets(bufferMax, TAMANHO_MAXIMO, stdin);
        bufferMax[strlen(bufferMax)-1] = '\0';

        if(bufferMax[0] == '\\'){
            checa_saida(bufferMax);
        }
        else {
            // Checa se a mensagem está ultrapassando o tamanho máximo permitido (4096)
            if(strlen(bufferMax) > TAMANHO_BUFFER){
                int tamanhoBuffer = strlen(bufferMax);
                int aux = 0;

                // Divide a mensagem até que o tamanho seja menor que o tamanho máximo permitido
                while(tamanhoBuffer > TAMANHO_BUFFER){

                    // Copia parte da mensagem para outro buffer, dividindo a mensagem completa
                    strncpy(buffer,bufferMax+aux, TAMANHO_BUFFER);
                    buffer[TAMANHO_BUFFER] = '\0';

                    if (aux == 0){
                        sprintf(message, " Servidor: %s", buffer);
                    } else {
                        sprintf(message, "\n Servidor: %s", buffer);
                    }
                    
                    // Envia a parte da mensagem que foi transferida para outro buffer
                    envia_mensagem(message, 0, true);

                    // Limpa o buffer
                    bzero(buffer, TAMANHO_BUFFER); 
                    bzero(message,TAMANHO_BUFFER+TAMANHO_USUARIO+2); 

                    // Como parte da mensagem foi transferida para outro buffer
                    // Decrementamos o tamanho dessa parte do tamanho total da mensagem
                    tamanhoBuffer -= TAMANHO_BUFFER;

                    // Incrementamos o auxiliar para que a próxima parte da mensagem seja transferida
                    aux += TAMANHO_BUFFER;
                }
                // Copia a última parte da mensagem para o buffer
                strncpy(buffer, bufferMax+aux, tamanhoBuffer);
            }
            else{
                // Se a mensagem não ultrapassar o tamanho máximo, envia a mensagem para todos os clientes sem a repartir
                strcpy(buffer, bufferMax);
            }
            // Envia a mensagem para todos os clientes
            sprintf(message, "Servidor: %s", buffer);
            envia_mensagem(message, 0, true); 
        }
    }
}


int main(int argc, char *argv[]){
    
    int clientSocket; // clientSocket irá armazenar o "socket descriptor" do usuario
    int portNum; // portNum irá armazenar a porta utilizada na conexão

    socklen_t clientAddressSize;   // clientAddressSize irá armazenar o tamanho da struct sockaddr_in do usuario
    struct sockaddr_in serverAddress, clientAddress; // serverAddress e clientAddress irão armazenar os endereços do usuario e do servidor
    
    char mensagemConectado[60] = "\nConexão realizada com sucesso\n";
    
    portNum = atoi(argv[1]);    

    // Cria o socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    // Verifica se o socket foi criado com sucesso
    // Se não foi, mostra a mensagem de erro
    if (serverSocket == -1){
        erro("Erro no Socket");
    }

    // Inicializa o serverAddress com zeros 
    bzero( (char *) &serverAddress, sizeof(serverAddress));  

    // Define o endereço do servidor
    serverAddress.sin_family = AF_INET;

    // Armazena o IP do servidor
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    // Armazena a porta do servidor - htons() formata a porta que é int para o formato correto
    serverAddress.sin_port = htons(portNum);   

    // opcaoSocket é o 4º argumento do setsockopt(), escolhendo o valor da opção do socket
    int opcaoSocket = 1;

    // setsockopt() é uma função que permite definir opções para o socket
    // O primeiro argumento é o socket descriptor, o segundo é a opção do socket
    // o terceiro é o valor da opção, e o quarto é o tamanho do valor da opção
    // setsockopt() retorna 0 se a opção foi definida com sucesso
    // Se não, retorna -1
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opcaoSocket, sizeof(int)) < 0){
        erro("Erro ao configurar o socket!");
    }

    // bind() irá atribuir um endereço/porta ao socket
    // O primeiro argumento é o socket descriptor, o segundo é o endereço do servidor
    // o terceiro é o tamanho do endereço do servidor
    // bind() retorna 0 se o bind foi realizado com sucesso
    if (bind(serverSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) == -1) 
    {
        erro("Erro ao definir nome para o socket!");
    }

    // Configura o socket para aceitar conexões até uma fila de 5 conexões pendentes
    // O primeiro argumento é o socket descriptor, o segundo é o tamanho da fila
    listen(serverSocket, 2); 

    printf("\nServer Iniciado!\n");
    printf("%s\n\nPara fechar o servidor, envie ""\\quit""\n", buffer);

    // Cria uma thread para enviar mensagens do servidor para todos
    std::thread sendMessagesToAll(gerencia_envio);

    // Separa a thread de execução para continuar sendo executada independentemente.
    sendMessagesToAll.detach();

    // Definir o tamanho do endereço do usuario
    // clientAddressSize é o tamanho da struct sockaddr_in do usuario
    clientAddressSize = sizeof(clientAddress);

    // Enquanto o servidor estiver rodando
    while(!serverOFF){ 

        // accept() é uma função que irá aceitar conexãos em um socket 
        // accept() retorna o socket descriptor do usuario
        clientSocket = accept(serverSocket, (struct sockaddr *) &clientAddress, &clientAddressSize);    
        
        // Verifica se o socket foi criado com sucesso
        if (clientSocket == -1){
            erro("Falha na conexão, erro ao criar socket\n");
        } else { 

            // Caso o socket tenha sido criado com sucesso
            if(clientSocket >= 0){ 

                // Escreve uma mensagem para o servidor
                write(clientSocket, mensagemConectado, strlen(mensagemConectado));

                // Declara as informações do usuario
                clients[qtdUsuarios].endereco = clientAddress;
                clients[qtdUsuarios].socketID = clientSocket;
                clients[qtdUsuarios].conectado = true;
                clients[qtdUsuarios].clientID = qtdUsuarios;

                // Inicializa uma thread para o usuario
                std::thread usuario (configurar_cliente, clients[qtdUsuarios]);

                // Separa a thread de execução para continuar sendo executada independentemente.
                usuario.detach();
            }
        }       

        // Se o servidor for desligado, retorna 0
        if(serverOFF){
            return 0;
        }

    }

    // Com o servidor desligado, fecha os sockets de usuario e servidor
    for (int i = 0; i < qtdUsuarios; i++){
        close(clients[i].socketID);
    }
    close(serverSocket);

    return 0; 
}