#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <thread>
#include <pthread.h>
#include <mutex>
#include <atomic>
#include <signal.h>
#include <algorithm> 
#include <list>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <thread>
#include <termios.h> 
#include <unistd.h>
#include <iostream> 
using namespace std;

#define MAXIMO_USUARIOS 10   
#define TAMANHO_BUFFER 4096 
#define TAMANHO_USUARIO 50 
#define TAMANHO_MAXIMO_BUFFER 40960 
#define TAMANHO_CANAL 50  

typedef struct USUARIO usuario;
typedef struct CANALTEXTO canal;

int quantidade_usuarios = 0; 
int quantidade_canais_criados = 0;            

char buffer[TAMANHO_BUFFER];  
char bufferMaximo[TAMANHO_MAXIMO_BUFFER+10]; 
char mensagem_texto[TAMANHO_BUFFER+TAMANHO_USUARIO+2]; 
bool disponivel = true; 
int socketServidor ; 
static bool nome_valido = false; 
std::mutex mtx; 
std::atomic<bool> flag (false); 

// struct que aramzenará informações do usuário
struct USUARIO {
    int ID_socket;
    struct sockaddr_in endereco;
    char nome[TAMANHO_USUARIO];
    int ID_usuario;
    bool conectado;
    char channelName[TAMANHO_CANAL];
    bool administrador_do_canal = false;
};

// struct que aramzenará informações do canal de texto
struct CANALTEXTO {
    char nome[TAMANHO_CANAL];
    USUARIO admin;                   
    bool active;
    std::list <USUARIO> usuarios_silenciados;   
    std::list <USUARIO> conectado;   
};

std::vector<USUARIO> clients; 
std::vector<CANALTEXTO> channels;   

void erro(const char *msg){
    perror(msg);
    exit(1);
}


// Função que checa se o usuário é administrator do servidor
bool administrador_do_canal(USUARIO usuario){
    // Percorre os canais existentes para achar o canal cujo usuário está presente
    for (int i = 0; i < quantidade_canais_criados; i++){
        // Ao achar o canal do usuário, checa se ele é o administrador
        if (strcmp (usuario.channelName, channels[i].nome) == 0){
            // Se o usuário for administrador do canal, retorna true
            if (channels[i].admin.ID_usuario == usuario.ID_usuario){
                return true;
            }
        }
    }
    return false;
}



// Função para checar o comando /quit que se desconecta do servidor
void checa_quit(char mensagem_texto[]){
    // Checa se o servidor quer sair do servdor
    // Se sim, fecha o socket e sai do programa
    // Se não, não faz nada
    if (strcasecmp(mensagem_texto, "/quit") == 0){    
        flag = true;
        // Envia mensagem de saída aos usuários
        printf("\n Saindo do servidor... \n\n");

        
        for (int j = 0; j < quantidade_usuarios; j++) {
            close(clients[j].ID_socket);
        }
        // Fecha o socket do servidor
        close(socketServidor );
        // Sai do programa
        exit(0);
    }
}

// Função que recebe o nome de um canal e o nome de usuário, percorre os usuários conectados ao canal fornecido
// e retorna as informações do usuário desejado
USUARIO *retorna_cliente(CANALTEXTO canal, char* nome){
    // Percorre os usuários do canal para encontrar o usuário com o nome especificado
    for (auto i = canal.conectado.begin(); i != canal.conectado.end(); i++) {
        // Se o nome do usuário for o mesmo que o nome especificado
        // retorna o ponteiro para o usuário
        if (strcmp((*i).nome, nome) == 0){
            return &(*i);
        }
    }

    // Se o usuário não for encontrado, retorna NULL
    return NULL;
}

// Nas especificações, foi pedido para que o usuário não possa sair do usuário utilizando 
// "ctrl" + "c", então foi criado um handler para isso.
// O handler é chamado quando o usuário pressiona "ctrl" + "c"
void checa_atalho_saida(int sig_num){
    signal(SIGINT, checa_atalho_saida); 
    // Informa ao usuário que para sair do servidor, ele deve utilizar /quit ou ctrl+D
    printf("Para sair do servidor, digite '/quit' ou use o comando  'Ctrl'+'D'\n");
    fflush(stdout); 
} 


// função para criar um novo canal de texto
void criar_canal(char* nome, USUARIO *admin){
    strcpy(admin->channelName, nome);
    admin->administrador_do_canal = true;
    CANALTEXTO c;

    strcpy(c.nome, nome);
    c.admin = *admin;

    c.conectado.push_back(*admin);

    channels.push_back(c);
    quantidade_canais_criados++;
}


// função que confere se um usuário está mutado ou não
bool checar_silenciamento(USUARIO usuario){

    // Se o usuário estiver silenciado, retorna true
    for (int i = 0; i < quantidade_canais_criados; i++){
        if (strcmp (usuario.channelName, channels[i].nome) == 0){
            for (auto j = channels[i].usuarios_silenciados.begin(); j != channels[i].usuarios_silenciados.end(); j++){
                if ((*j).ID_usuario == usuario.ID_usuario){
                    // Se o ID do usuário estiver na lista de silenciados, retorna true
                    return true;
                }
            } 
        }
    }

    // Se o usuário não estiver silenciado, retorna false
    return false;
}



// Função responsável por enviar mensagens, cumprindo as especificações contidas no enunciado do trabalho
void envia_mensagem(char* mensagem_texto, USUARIO usuario, bool enviarParaTodos) {


    // trava o mutex
    mtx.lock();

    if(enviarParaTodos == false){

        CANALTEXTO chat;
        
        for (auto i = channels.begin(); i != channels.end(); i++){
            // Checa se o canal é o mesmo do usuário
            if (strcmp((*i).nome, usuario.channelName) == 0){
                chat = (*i);
            }
        }

        // Checa se o usuário está silenciado
        if(!checar_silenciamento(usuario)){
            
            // Se chat.nome não for nulo, realiza  printf da mensagem para o canal
            if(chat.nome != NULL) printf("[%s] ", chat.nome);
            printf("%s\n", mensagem_texto); 

            // Envia mensagem para todos os usuários conectados no canal
            for (auto i = chat.conectado.begin(); i != chat.conectado.end(); i++){
                // Se o usuário não for o mesmo que enviou a mensagem
                if (((*i).ID_usuario != usuario.ID_usuario) && ((*i).conectado == true)){
                    bool ja_enviado = false;
                    int contador = 0;

                    while(ja_enviado == false && contador < 5){
                        if (nome_valido == false 
                        && write((*i).ID_socket, mensagem_texto, strlen(mensagem_texto)) < 0) {                
                            contador++;
                        }
                        else ja_enviado = true;
                    }

                    // Se o usuário não conseguiu enviar a mensagem depois de 5 tentativas, envia mensagem de erro
                    if(contador >= 5 && ja_enviado == false){
                        erro("Erro: Mensagem não enviada\n");
                        close((*i).ID_usuario);
                    }
                }
            }
        }
        else{
            // Se o usuário estiver silenciado, envia mensagem de alerta ao usuário
            char msgMuted[100] = "\nVocê está silenciado, e portanto não pode enviar mensagens.\n";
            // Envia mensagem de alerta ao usuário
            write(usuario.ID_socket, msgMuted, strlen(msgMuted));
        }
    }
    // Se "enviar para todos" for verdadeiro, então envia a mensagem para todos os usuários do servidor
    else{
        for(auto i = clients.begin(); i != clients.end(); i++){
             if (((*i).conectado == true)){
                    bool ja_enviado = false;
                    int contador = 0;
        
                    while(ja_enviado == false && contador < 5){
                        if (write((*i).ID_socket, mensagem_texto, strlen(mensagem_texto)) < 0 
                        && nome_valido == false) {                
                            contador++;
                        }
                        else ja_enviado = true;
                    }

                    if(contador >= 5 && ja_enviado == false){
                        erro("Erro: Mensagem não enviada\n");
                        close((*i).ID_usuario);
                    }
                }
        }
    }

    // Libera o mutex
    mtx.unlock();
}



// Função que checa as entradas do usuário, tratando cada comando iniciado em '/'
void checa_entrada_usuario(char* mensagem_texto, USUARIO *usuario){

    if (strcasecmp(mensagem_texto, "/ping") == 0){
        // Envia mensagem de pong ao usuário
        write(usuario->ID_socket, "pong\n", sizeof("pong\n"));     
    }
   
    // Comando de join, caso o usuário queira entrar em algum servidor
    if (strncasecmp(mensagem_texto, "/join ", 6) == 0){     

        // Aloca memória para o nome do canal que o usuário deseja entrar
        char* channelName = (char*)malloc(sizeof(char)*50);
        // Pega o nome do canal que o usuário deseja entrar
        strcpy(channelName, mensagem_texto + strlen("/join "));
        
        // Checa se o nome do canal é válido, se não for, envia mensagem de erro
        if((channelName[0] == '#' || channelName[0] == '&') && strlen(channelName) <= 50){

            bool aux = true;
            // Percorre os canais existentes para verificar se o canal existe
            for (auto i = channels.begin(); i != channels.end(); i++){


                // Checa se o nome informado pelo usuario consta na lista de canais
                if (strcmp((*i).nome, channelName) == 0){      

                    // Se o canal existir, então o usuário entra no canal
                    strcpy (usuario->channelName, channelName);
                    usuario->administrador_do_canal = false;

                    // Cria mensagem de texto para enviar ao usuário
                    char mensagemBoasVindas[50];
                    sprintf(mensagemBoasVindas, "\n Olá! Seja bem vindo ao canal %s!\n\n", channelName);
                    write(usuario->ID_socket, mensagemBoasVindas, strlen(mensagemBoasVindas));

                    // Envia mensagem informando que o usuário entrou no canal
                    char joinChatMsgAll[100];
                    sprintf(joinChatMsgAll, "\n\nO usuário %s acaba de entrar no canal %s!\n\n", usuario->nome, channelName);
                    printf("%s", joinChatMsgAll);
                    // Envia mensagem a todos os usuários do canal
                    envia_mensagem(joinChatMsgAll, *usuario, false);

                    
                    (*i).conectado.push_back(*usuario);        
                    aux = false;
                    break;
                }
            }

            // Caso o canal não exista, então é criado um novo canal, e quem o criou é o seu administrador
            if (aux){
                // Cria um novo canal com o nome informado pelo usuário
                criar_canal(channelName, usuario); 
                char mensagem_canal_criado[121];
                sprintf(mensagem_canal_criado, "\nO canal %s informado não existia, portanto, você acaba de o criar! Como você criou o canal, você é o administrador.\n", channelName);
                
                write(usuario->ID_socket, mensagem_canal_criado, strlen(mensagem_canal_criado));
                char comandos_admin[961];
                sprintf(comandos_admin, "\n  ╔═════════════════════════════════════════════════════════════════════════════════╗\n  ║Comandos do administrador:                                                       ║\n  ║                                                                                 ║\n  ║   - /kick [username] (expulsa um usuário do chat)                               ║\n  ║   - /mute [username] (proíbe usuário de enviar mensagens no chat)               ║\n  ║   - /whois [username] (retorna o endereço IP apenas para o admin)               ║\n  ╚═════════════════════════════════════════════════════════════════════════════════╝\n\n");
                write(usuario->ID_socket, comandos_admin, strlen(comandos_admin));
                
                char aviso_canal_criado[100];
                sprintf(aviso_canal_criado, "\n O usuário %s criou o canal %s \n", usuario->nome, channelName);
                printf("%s", aviso_canal_criado);
            }

        }
        else{
            char mensagem_erro_conexao[250] = "\n - Erro ao tentar conectar em um servidor\n Verifique se você digitou corretamente o nome do servidor\n"
            "Lembre-se que o nome do servidor deve começar com '&' ou '#' e ter no máximo 50 caracteres, além de não conter espaços ou vírgulas.\n";
            write(usuario->ID_socket, mensagem_erro_conexao, strlen(mensagem_erro_conexao));
        }       

    }

    // Comando para alterar o nome do usuário
    else if (strncasecmp(mensagem_texto, "/nickname ", 10) == 0 ){ 

        strcpy(usuario->nome, mensagem_texto + strlen("/nickname "));
        write(usuario->ID_socket, "Nome atualizado.", strlen("Nome atualizado."));
    }
    // Comando para expulsar algum usuário
    else if(strncasecmp(mensagem_texto, "/kick ", 6) == 0){     
        // Checa se o usuário é administrador do canal
        if (usuario->administrador_do_canal){

            char nome[TAMANHO_USUARIO];
            CANALTEXTO aux;
            strcpy(nome, mensagem_texto + strlen("/kick "));

            // Percorre os usuários do canal para encontrar o usuário que será expulso
            for (auto i = channels.begin(); i != channels.end(); i++){
               if (strcmp (usuario->channelName, (*i).nome) == 0){
                    aux = *i;
               }
            }

            USUARIO *ck = retorna_cliente(aux, nome);
            if(ck != NULL){
                // Envia mensagem ao usuário expulso
                write(ck->ID_socket, "Expulso do canal.\n", sizeof("Expulso do canal.\n"));
                printf("O usuário %s foi expulso\n", ck->nome);
                close(ck->ID_socket);
            }
            else{
            write(usuario->ID_socket, "\n\n O usuário informado não existe no seu canal, verifique se você digitou corretamente\n", strlen("\n\n O usuário informado não existe no seu canal, verifique se você digitou corretamente\n"));
            } 
        }
        else {
             write(usuario->ID_socket, "\n Você precisa ser o administrador do canal para isso.\n", strlen("\n Você precisa ser o administrador do canal para isso.\n"));
        }
    }
    
    else if(strncasecmp(mensagem_texto, "/mute ", 6) == 0){ 
        // Checa se o usuário é administrador do canal
        if (usuario->administrador_do_canal){
            // Declara a vriável que armazena o nome do usuário que será mutado
            char nome[TAMANHO_USUARIO];
            CANALTEXTO *canal_do_usuario;
            // Percorre a lista de canais para encontrar o canal que o usuário está
            for (auto i = channels.begin(); i != channels.end(); i++){
               if (strcmp (usuario->channelName, (*i).nome) == 0){
                    canal_do_usuario = &(*i);
               }
            }

            // Pega o nome do usuário que será mutado
            strcpy(nome, mensagem_texto + strlen("/mute "));

            // Caso o usuário informe o próprio nome, é enviada uma mensagem de erro,
            // pois não é possível um usuário mutar a si mesmo
            if(strcmp(usuario->nome, nome) == 0){
                printf("Você não pode silenciar você mesmo.");
                char mensagem_erro_autosilence[50] = "Você não pode silenciar você mesmo.";
                write(usuario->ID_socket, mensagem_erro_autosilence, strlen(mensagem_erro_autosilence));            
            }
            else{           
            
                
                // Percorre a lista de usuários do canal para encontrar o usuário que será mutado
                USUARIO *temp = retorna_cliente(*canal_do_usuario, nome);
                // Caso o usuário não exista, é enviada uma mensagem de erro
                if (temp == NULL){
                    write(usuario->ID_socket, "\nO usuário informado não existe\n", strlen("\nO usuário informado não existe\n"));
                }
                else {
                    // Com o usuário encontrado, envia uma mensagem para ele o alertando do silenciamento
                    canal_do_usuario->usuarios_silenciados.push_back(*temp);    
                    
                    char aviso_silenciado[100] = "\nO administrador do canal silenciou você\n\n";
                    write(temp->ID_socket, aviso_silenciado, strlen(aviso_silenciado));

                    char aviso_usuario_silenciado[100];
                    sprintf(aviso_usuario_silenciado, "\nVocê acabou de silenciar o usuário %s\n", temp->nome);
                    write(usuario->ID_socket, aviso_usuario_silenciado, strlen(aviso_usuario_silenciado));
                }
            }
        }
        else {
             write(usuario->ID_socket, "Você precisa ser o administrador do canal.", strlen("Você precisa ser o administrador do canal. "));
        }
    }

   
    else if(strncasecmp(mensagem_texto, "/unmute ", 8) == 0){   
        // Checa se o usuário é administrador do canal
        if (usuario->administrador_do_canal){
            // Aloca memória para o nome do usuário que será mutado
            char nome[TAMANHO_USUARIO];
            CANALTEXTO *canal_do_usuario;
            // Percorre a lista de canais para encontrar o canal que o usuário está
            for (auto i = channels.begin(); i != channels.end(); i++){
               if (strcmp (usuario->channelName, (*i).nome) == 0){
                    canal_do_usuario = &(*i);
               }
            }

            // Pega o nome do usuário que será mutado
            strcpy(nome, mensagem_texto + strlen("/unmute "));
            USUARIO *temp = retorna_cliente(*canal_do_usuario, nome);  
            // Caso o usuário não exista, é enviada uma mensagem de erro
            if (temp == NULL){
                write(usuario->ID_socket, "\nO usuário informado não existe, verifique se você digitou corretamente\n", strlen("\nO usuário informado não existe, verifique se você digitou corretamente\n"));
            }
            // Caso o usuário exista:
            else{
                for (auto i = canal_do_usuario->usuarios_silenciados.begin(); i != canal_do_usuario->usuarios_silenciados.end(); i++){
                    // Se o usuário encontrado estiver na lista de usuários mutados, ele é removido da lista de silenciados
                    if (strcmp (temp->nome, (*i).nome) == 0){
                        // Remove o usuário da lista de usuários silenciados (dessilencia)
                        canal_do_usuario->usuarios_silenciados.erase(i);
                        break;
                    }
                }

                char aviso_silenciado[100] = "\nO administrador do canal dessilenciou você.\n";
                write(temp->ID_socket, aviso_silenciado, strlen(aviso_silenciado));

                char mensagem_dessilenciar_usuario[100];
                sprintf(mensagem_dessilenciar_usuario, "\nVocê dessilenciou o usuário %s.\n\n", temp->nome);
                write(usuario->ID_socket, mensagem_dessilenciar_usuario, strlen(mensagem_dessilenciar_usuario));           
            }
                
            
        }
        // Caso o usuário não seja administrador do canal, é enviada uma mensagem de erro
        else {
            write(usuario->ID_socket, "Você precisa ser o administrador do canal.", strlen("Você precisa ser o administrador do canal. "));
        }
    }

    else if(strncasecmp(mensagem_texto, "/whois ", 7) == 0){    
        // Checa se o usuário é administrador do canal
        if (administrador_do_canal(*usuario)){
            char nome[TAMANHO_USUARIO];
            CANALTEXTO canal_do_usuario;
            // Percorre a lista de canais para encontrar o canal que o usuário está
             for (int i = 0; i < quantidade_canais_criados; i++){
                // Caso o usuário esteja no canal, o canal é salvo na variável canal_do_usuario
               if (strcmp (usuario->channelName, channels[i].nome) == 0){
                    canal_do_usuario = channels[i];
               }
             }
            //  Pega o nome do usuário a ser descoberto
            strcpy(nome, mensagem_texto + strlen("/whois "));
            USUARIO *temp = retorna_cliente(canal_do_usuario, nome);

            // Se o usuário não existir, envia uma mensagem de erro
            if (temp == NULL){
                write(usuario->ID_socket, "\nO usuário informado não existe.\n", strlen("\nO usuário informado não existe.\n"));
            }
            // Caso o usuário exista, envia mensagem com o IP e a Porta do usuário
            else{
                socklen_t endereco;
                endereco = sizeof(temp->endereco);
                // Função que retorna o endereço do usuário conectado ao socket informado
                getpeername(temp->ID_socket, (struct sockaddr*)&temp->endereco, &endereco);
                char ip[200];
                // Envia mensagem informando o IP e a Porta do usuário
                sprintf(ip, "\nO IP do usuário %s é: %s:%d\n", temp->nome, inet_ntoa((temp->endereco.sin_addr)), ntohs(temp->endereco.sin_port));
                write(usuario->ID_socket, ip, strlen(ip));
            }
        }
        // Caso o usuário não seja administrador do canal, é enviada uma mensagem de erro
        else {
            write(usuario->ID_socket, "Você precisa ser o administrador do canal.", strlen("Você precisa ser o administrador do canal."));
        }
    }
}



void configura_cliente(USUARIO usuario){
    
    char nick[TAMANHO_USUARIO];

    nome_valido = false;
    quantidade_usuarios++;

    if(recv(usuario.ID_socket, nick, TAMANHO_USUARIO, 0) <= 0){
        erro("Erro ao receber dados do socket do usuário\n");
        nome_valido = true;
    }
    else{
        strcpy(usuario.nome, nick);

        sprintf(mensagem_texto, "\n%s acaba de se conectar no chat.\n\n", usuario.nome);
        printf("%s", mensagem_texto);
    }
    
    bzero(mensagem_texto, TAMANHO_BUFFER+TAMANHO_USUARIO+2); 
    
    while(!nome_valido){ 

        bzero(mensagem_texto, TAMANHO_BUFFER+TAMANHO_USUARIO+2); 

        // Recebe os dados do socket 
        int dadoDoSocket  = recv(usuario.ID_socket, mensagem_texto, TAMANHO_BUFFER+TAMANHO_USUARIO+2, 0);

        if(dadoDoSocket  > 0){
            if (strlen(mensagem_texto) > 0) {

                if(mensagem_texto[0] == '/')
                    checa_entrada_usuario(mensagem_texto, &usuario);

                else{
                    char texto_mensagem[TAMANHO_BUFFER+TAMANHO_USUARIO];
                    sprintf(texto_mensagem, "%s: %s", usuario.nome, mensagem_texto);
                    envia_mensagem(texto_mensagem, usuario, false);
                    
                }
            }
        }
        else if (dadoDoSocket  == 0 || strcasecmp("quit\n", mensagem_texto) == 0) {
            sprintf(mensagem_texto, "\n%s se desconectou do canal.\n\n", usuario.nome);
            printf("%s", mensagem_texto);

            envia_mensagem(mensagem_texto, usuario, false);

            nome_valido = true; 
        } else {
            erro("Erro com o usuário.\n");
            nome_valido = true; 
        }
        bzero(mensagem_texto, TAMANHO_BUFFER+TAMANHO_USUARIO+2); 
    }

    // Desconecta o usuário do servidor
    usuario.conectado = false;
    // Remove o usuário da lista de usuários
    close(usuario.ID_socket);
    // Decrementa a quantidad ede usuários conectados
    quantidade_usuarios--;
    disponivel = false;
}

void configura_envio(){
	string s;

    // Enquanto o servidor estiver rodando
    while(!flag){
        
        // Limpa os buffers 
        bzero(bufferMaximo,TAMANHO_MAXIMO_BUFFER); 
        bzero(mensagem_texto,TAMANHO_BUFFER+TAMANHO_USUARIO+2); 
        // Lê a mensagem de entrada 
        fgets(bufferMaximo, TAMANHO_MAXIMO_BUFFER, stdin); 
        bufferMaximo[strlen(bufferMaximo)-1] = '\0';
        
        if (feof(stdin)){
            strcpy(bufferMaximo, "/quit");
        }
        if(bufferMaximo[0] == '/'){
            checa_quit(bufferMaximo);
        }
        else{
            if(strlen(bufferMaximo) > TAMANHO_BUFFER){
                int tam = strlen(bufferMaximo);
                int aux = 0;
            
                // Divide a mensagem até que o tamanho seja menor que o tamanho máximo permitido
                while(tam > TAMANHO_BUFFER){
                    // Copia parte da mensagem para outro buffer, dividindo a mensagem completa
                    strncpy(buffer, bufferMaximo+aux, TAMANHO_BUFFER);
                    buffer[TAMANHO_BUFFER] = '\0';

                    if (aux == 0)
                        sprintf(mensagem_texto, " Servidor: %s", buffer);
                    else sprintf(mensagem_texto, "\n Servidor: %s", buffer);
                    
                    USUARIO c;
                    // Envia a parte da mensagem que foi transferida para outro buffer
                    envia_mensagem(mensagem_texto, c, true);
                    // Limpa o buffer
                    bzero(buffer, TAMANHO_BUFFER); 
                    bzero(mensagem_texto,TAMANHO_BUFFER+TAMANHO_USUARIO+2); 

                    // Como parte da mensagem foi transferida para outro buffer
                    // Decrementamos o tamanho dessa parte do tamanho total da mensagem
                    tam -= TAMANHO_BUFFER;

                    // Incrementamos o auxiliar para que a próxima parte da mensagem seja transferida
                    aux += TAMANHO_BUFFER;
                }
                // Copia a última parte da mensagem para o buffer
                strncpy(buffer, bufferMaximo+aux, tam);
            }
            else{
                // Se a mensagem não ultrapassar o tamanho máximo, envia a mensagem para todos os clientes sem a repartir
                strcpy(buffer, bufferMaximo);
            }
            // Envia a mensagem para todos os clientes
            sprintf(mensagem_texto, "Servidor: %s\n", buffer);
            USUARIO c;
            envia_mensagem(mensagem_texto, c, true); 
        }
    }
}


int main(int argc, char *argv[]){
    int socketUsuario; // socketUsuario irá armazenar o "socket descriptor" do usuario
    int numeroPorta; // numeroPorta irá armazenar a porta utilizada na conexão

    socklen_t tamanhoEnderecoUsuario;   // tamanhoEnderecoUsuario irá armazenar o tamanho da struct sockaddr_in do usuario
    struct sockaddr_in enderecoServidor, enderecoUsuario; // enderecoServidor e enderecoUsuario irão armazenar os endereços do usuario e do servidor
    
    char mensagemConectado[60] = "\nConexão realizada com sucesso\n\n";
    

    signal(SIGINT, checa_atalho_saida);
    signal(SIGQUIT,checa_atalho_saida);
    numeroPorta = atoi(argv[1]);    

    // Cria o socket
    socketServidor = socket(AF_INET, SOCK_STREAM, 0);

    // Verifica se o socket foi criado com sucesso
    // Se não foi, mostra a mensagem de erro
    if (socketServidor == -1){
        erro("Erro no Socket");
    }

    // Inicializa o enderecoServidor com zeros 
    bzero( (char *) &enderecoServidor, sizeof(enderecoServidor));  

    // Define o endereço do servidor
    enderecoServidor.sin_family = AF_INET;

    // Armazena o IP do servidor
    enderecoServidor.sin_addr.s_addr = INADDR_ANY;
    // Armazena a porta do servidor - htons() formata a porta que é int para o formato correto
    enderecoServidor.sin_port = htons(numeroPorta);   

    // opcaoSocket é o 4º argumento do setsockopt(), escolhendo o valor da opção do socket
    int opcaoSocket = 1;

    // setsockopt() é uma função que permite definir opções para o socket
    // O primeiro argumento é o socket descriptor, o segundo é a opção do socket
    // o terceiro é o valor da opção, e o quarto é o tamanho do valor da opção
    // setsockopt() retorna 0 se a opção foi definida com sucesso
    // Se não, retorna -1
    if (setsockopt(socketServidor , SOL_SOCKET, SO_REUSEADDR, &opcaoSocket , sizeof(int)) < 0){
        erro("Erro ao configurar o socket!");
    }

    // bind() irá atribuir um endereço/porta ao socket
    // O primeiro argumento é o socket descriptor, o segundo é o endereço do servidor
    // o terceiro é o tamanho do endereço do servidor
    // bind() retorna 0 se o bind foi realizado com sucesso
    if (bind(socketServidor , (struct sockaddr *) &enderecoServidor, sizeof(enderecoServidor)) == -1){
        erro("Erro ao definir nome para o socket!");
    }

    // Configura o socket para aceitar conexões até uma fila de 5 conexões pendentes
    // O primeiro argumento é o socket descriptor, o segundo é o tamanho da fila
    listen(socketServidor, 5); 
                        

    printf("\nServer Iniciado!\n");
    printf("%s\n\nPara fechar o servidor envie o comando \\quit ou use o comand o 'CTRL' + 'D' \n", buffer);

    
    // Cria uma thread para enviar mensagens do servidor para todos
    thread threadEnviaMensagens(configura_envio);

    // Separa a thread de execução para continuar sendo executada independentemente.
    threadEnviaMensagens.detach();

    // Definir o tamanho do endereço do usuario
    // tamanhoEnderecoUsuario é o tamanho da struct sockaddr_in do usuario
    tamanhoEnderecoUsuario = sizeof(enderecoUsuario);

    // Enquanto o servidor estiver rodando
    while(!flag){ 
        
        // accept() é uma função que irá aceitar conexãos em um socket 
        // accept() retorna o socket descriptor do usuario
        socketUsuario = accept(socketServidor , (struct sockaddr *) &enderecoUsuario, &tamanhoEnderecoUsuario);

        if(MAXIMO_USUARIOS == (quantidade_usuarios)){ 
            if(disponivel){
                printf("\nUm usuário tentou se conectar em um canal cheio\n");                
                write(socketUsuario, "Canal cheio, não aceita mais participantes.", strlen("Canal cheio, não aceita mais participantes."));
                close(socketUsuario);
            }
        }
        
        // Verifica se o socket foi criado com sucesso
        else if (socketUsuario == -1){
            erro("Falha na conexão, erro ao criar socket\n");
        }else {       

            // Caso o socket tenha sido criado com sucesso
            if(socketUsuario >= 0){ 

                // Escreve uma mensagem para o servidor
                write(socketUsuario, mensagemConectado, strlen(mensagemConectado));    

                USUARIO usuarios;
                
                // Declara as informações do usuario
                usuarios.endereco = enderecoUsuario;
                usuarios.ID_socket = socketUsuario;
                usuarios.conectado = true;
                usuarios.ID_usuario = quantidade_usuarios;

                clients.push_back(usuarios);

                // Inicializa uma thread para o usuario
                std::thread usuario (configura_cliente, clients[quantidade_usuarios]);

                // Separa a thread de execução para continuar sendo executada independentemente.
                usuario.detach();
            }
        }       

        if(flag){
            return 0;
        } 

        sleep(2);
    }

    printf("\nDesligando o servidor\n\n");

    // Com o servidor desligado, fecha os sockets de usuario e servidor
    for (int i = 0; i < quantidade_usuarios; i++){
        close(clients[i].ID_socket);
    }
    // Fecha o socket do servidor 
    close(socketServidor ); 

    return 0; 
}
