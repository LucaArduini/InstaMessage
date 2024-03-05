#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"

typedef uint8_t bool;
#define true 1
#define false 0

#define BUFFER_SIZE 1024
#define INFO_SIZE 20

char dummy_Notification[110] = "";
char notification_1[110] = "";
char notification_2[110] = "";
char notification_3[110] = "";

///---------------VARIABILI GLOBALI--------------------
struct User* addressBook = NULL;
struct Chat* myChats = NULL;

char myusername[INFO_SIZE];
char mypassword[INFO_SIZE];
char buffer[BUFFER_SIZE];
bool inChat = false;
char inChatWith[INFO_SIZE];
uint8_t buf8;
uint16_t my_port, srv_port, buf16;
uint32_t buf32;
int sd_server=-1, ret;
bool isServerOnline = true;

fd_set master;
int listener, fdmax;

///---------------STRUCT-------------------------------
struct User{
    char username[INFO_SIZE];
    bool online;
    int sd_chat;
    struct User* next;
};
struct Msg{
    char name_dest[INFO_SIZE];
    char name_source[INFO_SIZE];
    char* text;
    uint8_t state;                  //1) non recapitato     2)recapitato
    struct Msg* next;
};
struct Member{
    struct User* userPointer;
    struct Member* next;
};
struct Chat{
    char name_chat[INFO_SIZE];      //nel caso di chat privata è il nome di un utente, nel caso di chat di gruppo è il nome del gruppo
    bool isGroup;
    uint16_t numPart;               //utente locale non incluso
    struct Member* members;
    struct Msg* messages;
    struct Chat* next;
};

///----------FUNZIONI PRINCIPALI (dichiarazione)-------
uint8_t signup_d();
uint8_t in_d();
void chat_d(char*);
void hanging_d();
void show_d(char*);
void share(struct Chat*, char*);
void out_d();

void enterInChatMode(struct Chat*);

///----------FUNZIONI AUSILIARIE (dichiarazione)-------
int newListenerSocket();
int newCommunicationSocket();
int connect_to_a_server(uint16_t);
void disconnect_from_a_server(int*);
void readAddressBook();
void login();
void showMenu();
void showChatInterface(struct Chat*);
void markAsGiven();
void addMemberGroup(char*, struct Chat*);
void receiveAddMemberGroup(int);
void receiveGroupInvite(int);
void leaveGroup_me(struct Chat*);
void deleteChat(struct Chat*);
void removeMemberfromALLGroups(struct User*);
bool removeMemberfromSpecificGroup(struct Chat*, char*);
void listOnlineFriends();
void askIfOnline_d(struct User*);
uint16_t askPort_d(struct User*);
void newNotification();
void clearNotification();
void receiveFile(int);

///----------FUNZIONI DI UTILITÀ (dichiarazione)-------
void sendMsg(struct Chat*);
void sendFirstMsg(struct User*, struct Chat*);
void sendMsg_to_server(struct User*);
bool isMyFirstOfflineMsg(struct Chat*);
bool recvMsg(int);
bool receiveFirstMsg();
void sendText(int);
void recvText(int);
void sendInfo(int, char*);
void recvInfo(int);
void sendNumber(int, void*, uint8_t);
void recvNumber(int, uint8_t);
struct User* newUser(char*, bool);
struct Msg* newMsg(char*, char*, char*, uint8_t);
struct Chat* pushNewChat(struct User*);
void pushNewMember(struct Chat*, struct User*);
void pushUser(struct User**, struct User*);
void pushMsg(struct Chat*, struct Msg*);
struct User* findUser(char*);
struct User* findUser_by_sd(int);
struct Chat* findChat(char*);



int main(int argc, char* argv[]){
    char command[8], option[INFO_SIZE];
    struct User *p_user, tmp_user;
    int i;
    fd_set read_fds;

    //-------------INIZIO-------------------
    my_port = (argc>1? atoi(argv[1]) : 5007);

    listener = newListenerSocket();
    login();
    readAddressBook(); 
    showMenu();

    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_SET(0, &master);                                                 //descrittore dello standard input
    FD_SET(listener, &master);
    FD_SET(sd_server, &master);                                         //descrittore del socket che uso per comunicare con il server
    fdmax = (listener > sd_server)? listener : sd_server;

    for(;;){
        read_fds = master;
        ret = select(fdmax +1, &read_fds, NULL, NULL, NULL);
        if(ret<0){ perror("ERRORE SELECT"); exit(-1); }

        for(i=0; i<=fdmax; i++) {
            if(!FD_ISSET(i, &read_fds))
                continue;
//STANDARD INPUT:
            if(i == 0){
                fgets(buffer, BUFFER_SIZE, stdin);
                sscanf(buffer, "%s %s", &command[0], &option[0]);

                if( strcmp(command, "hanging") == 0 ){
                    hanging_d();
                }
                else if( strcmp(command, "show") == 0 ){
                    show_d(option);
                }
                else if( strcmp(command, "chat") == 0 ){
                    chat_d(option);
                }
                else if( strcmp(command, "out") == 0 ){
                    out_d();
                    return 0;                                           //Chiudo l'applicazione
                }
                else{
                    puts("> il comando digitato NON ESISTE, ritenta\n");
                }
            }
//SOCKET ASCOLTO
            else if(i == listener){
                tmp_user.sd_chat = newCommunicationSocket();
                recvInfo(tmp_user.sd_chat);
                strncpy(tmp_user.username, buffer, strlen(buffer)+1);

                p_user = findUser(tmp_user.username);
                if(p_user){                                             //la richiesta diretta al listener proviene da un contatto salvato in rubrica
                    p_user->online = true;
                    p_user->sd_chat = tmp_user.sd_chat;
                }
                else{                                                   //richiesta arrivata da un contatto NON in rubrica, lo aggiungo alla rubrica TEMPORANEA
                    pushUser(&addressBook, newUser(tmp_user.username, true));
                }
            }
//SERVER PRINCIPALE
            else if(i == sd_server){
                ret = recv(sd_server, (void*) &buf8, sizeof(uint8_t), 0);

                if(ret == 0){
                    sprintf(dummy_Notification, ">>! NOTIFICA: il SERVER è andato offline.");
                    newNotification();
                    isServerOnline = false;
                    close(i); FD_CLR(i, &master);
                }
                else if(ret < 0){
                    perror("ERRORE!");
                    close(i); FD_CLR(i, &master);
                }
                else{
                    switch(buf8){
                        case 1:     //receiveFirstMsg
                            receiveFirstMsg();
                            break;
                        case 2:     //il server mi dice che un utente ha fatto la show di alcuni miei messaggi pendenti
                            markAsGiven();
                            break;
                    }
                }
            }
//SOCKET COMUNICAZIONE            
            else {
                ret = recv(i, (void*) &buf8, sizeof(uint8_t), 0);

                if(ret == 0){                                       //rilevata chiusura di un client
                    p_user = findUser_by_sd(i);
                    p_user->online = false;
                    p_user->sd_chat = -1;
                    removeMemberfromALLGroups(p_user);
                    close(i); FD_CLR(i, &master);
                }
                else if(ret < 0){
                    perror("ERRORE!");
                    close(i); FD_CLR(i, &master);
                }
                else{                                               //un device mi ha inviato un msg
                    p_user = findUser_by_sd(i);
                    switch(buf8){
                        case 1:     //messaggio in arrivo
                            recvMsg(i);
                            break;
                        case 2:     //addMember in arrivo
                            receiveAddMemberGroup(i);
                            break;
                        case 3:     //group invite in arrivo
                            receiveGroupInvite(i);
                            break;
                        case 4:     //file in arrivo
                            receiveFile(i);
                            break;
                    }
                }
            }
        }
    } // Fine for(;;)

}   ///-----FINE MAIN-----



///----------FUNZIONI PRINCIPALI (implementazione)---------
uint8_t signup_d(){
    sendInfo(sd_server, myusername);
    sendInfo(sd_server, mypassword);
    recvNumber(sd_server, 8);                                           //ricevo l'esito dell'autentificazione

    if(buf8 == 1)
        puts("> operazione andata a buon fine. Ora sei pronto per accedere al tuo account\n");
    else
        puts("> operazione fallita. Username già usato, riprova usandone uno nuovo\n");

    disconnect_from_a_server(&sd_server);
    return 1;                                                           //a prescindere dall'esito dovrò comunque ripetere il login
}
uint8_t in_d(){
    sendInfo(sd_server, myusername);
    sendInfo(sd_server, mypassword);
    recvNumber(sd_server, 8);                                           //ricevo l'esito dell'autentificazione

    switch(buf8){
    case 0:
        puts("> l'username digitato non è registrato\n");
        return 1;
    case 1:
        puts("> password errata, riprova\n");
        return 1;
    case 2:
        //login riuscito (il messaggio di avvenuto login lo faccio già nel menu iniziale)
        sendNumber(sd_server, &my_port, 16);
        return 0;
    default:                                                            //non dovrei averlo mai, ma non definirlo mi genera un warning
        return 1;
    }   
}
void chat_d(char* name_chat){
    struct User* p_dest = findUser(name_chat);
    struct Chat* p_chat = findChat(name_chat);

    if(!p_chat && !p_dest){                                             //controllo se il contatto digitato è nella mia rubrica (o se non è un gruppo)
        printf("> l'utente \"%s\" non è presente nella tua rubrica\n\n", name_chat);
        return;
    }
    if(!p_chat && !isServerOnline){
        printf("> purtroppo il server è offline → non si può iniziare una NUOVA chat\n> Premi INVIO per proseguire\n");
        getchar();
        showMenu();
        return;
    }
    if(!p_chat)                                                         //il contatto è nella rubrica, ma non abbiamo ancora chattato
        p_chat = pushNewChat(p_dest);
    
    enterInChatMode(p_chat);
}
void hanging_d(){
    char username_source[INFO_SIZE];
    char buffer_time[INFO_SIZE];
    uint16_t counter;
    int i;

    buf8 = 4;   sendNumber(sd_server, &buf8, 8);                        //dico al server che voglio fare hanging
    sendInfo(sd_server, myusername);

    recvNumber(sd_server, 16);                                          //ricevo numero dei source
    counter = buf16;
    if(counter == 0){
        printf("> Non hai ricevuto nuovi messaggio mentre eri offline.\n\n");
    }
    else{
        for(i=0; i < counter; i++){
            recvInfo(sd_server);            //ricevo username_source
            strncpy(username_source, buffer, strlen(buffer)+1);
            recvNumber(sd_server, 16);      //ricevo msgCounter
            recvInfo(sd_server);            //ricevo timestamp
            strncpy(buffer_time, buffer, strlen(buffer)+1);
            printf("> %s\t\tnum msg: %hd\tultimo msg: %s\n", username_source, buf16, buffer_time);
        }
        puts("");
    }
}
void show_d(char* username_source){
    struct Chat* p_chat = findChat(username_source);
    struct User* p_dest = findUser(username_source);
    int i;
    uint16_t counter;

    if(!p_chat && !p_dest){                                             //controllo se il contatto digitato è nella mia rubrica (o se non è un gruppo)
        printf("> l'utente \"%s\" non è presente nella tua rubrica\n\n", username_source);
        return;
    }
    if(!p_chat)                                                         //se ancora non ho inizializzato una chat con dest, lo faccio ora
        p_chat = pushNewChat(p_dest);

    buf8 = 5;   sendNumber(sd_server, &buf8, 8);                        //chiedo al server la show, gli invio il mio username e quello del mittente
    sendInfo(sd_server, myusername);
    sendInfo(sd_server, username_source);

    recvNumber(sd_server, 16);                                          //ricevo numero messaggi in arrivo
    counter = buf16;

    if(!inChat){        //show non chiamata in schermata chat
        switch(counter){
        case 0:
            printf("> Non hai ricevuto nuovi messaggi da %s mentre eri offline.\n\n", username_source);
            return;
        case 1:
            printf("> L'utente %s ti ha inviato 1 nuovo messaggio mentre eri offline:\n", username_source);
            break;
        default:
            printf("> L'utente %s ti ha inviato %d messaggi mentre eri offline:\n", username_source, counter);
            break;
        }

        for(i=0; i < counter; i++){
            recvText(sd_server);
            printf("  > %s: ** %s\n", username_source, buffer);
            pushMsg(p_chat, newMsg(myusername, username_source, buffer, 2));
        }
        puts("");
    }
    else{               //se questa funzione viene chiamata in una chat, mi basta solo caricare i messaggi
        for(i=0; i < counter; i++){
            recvText(sd_server);
            pushMsg(p_chat, newMsg(myusername, username_source, buffer, 2));
        }
    }
}
void share(struct Chat* p_chat, char* name_file){
    struct Member* p_m;
    FILE* fp;
    char pathFile[77], *res;

    if((strncmp(name_file, "./", 2) == 0) || (strncmp(name_file, "../", 3) == 0)){              //controllo dell'input
        printf("> NO, non puoi navigare tra le cartelle ;P\n> Premi INVIO per proseguire\n");
        getchar();
        showChatInterface(p_chat);
        return;
    }
    else
        sprintf(pathFile, "files_%s/%s", myusername, name_file);

    fp=fopen(pathFile, "r");
    if(strcmp(name_file, "")==0 || fp==NULL){ 
        printf("> ATTENZIONE file \"%s\" non trovato.\n> Premi INVIO per proseguire\n", name_file);
        getchar();
        showChatInterface(p_chat);
        return;
    }

    sprintf(buffer, "<< sent the file \"%s\" >>", name_file);                                   //comunico ai mittenti l'arrivo di un file
    sendMsg(p_chat);
    showChatInterface(p_chat);

    //dico ai partecipanti del gruppo che sta per arrivare un file
    for(p_m=p_chat->members; p_m; p_m=p_m->next){
        buf8=4; sendNumber(p_m->userPointer->sd_chat, &buf8, 8);        //dico al dest che sta per arrivare un file
        sendInfo(p_m->userPointer->sd_chat, p_chat->name_chat);         //invio nome chat di destinazione
        sendInfo(p_m->userPointer->sd_chat, myusername);                //invio username mittente (il mio)
        strcpy(buffer, name_file);                                      //copio il nome del file nel buffer
        sendText(p_m->userPointer->sd_chat);                            //invio il nome del file

        rewind(fp);                                                     //riporto il puntatore all'inizio del file             

        while(1){
            res = fgets(buffer, BUFFER_SIZE, fp);                       //leggo una porzione di file e la invio al destinatario
            if(!res)
                break;

            sendText(p_m->userPointer->sd_chat);
        }

        strcpy(buffer, "<< end-of-file >>");
        sendText(p_m->userPointer->sd_chat);
    }

    fclose(fp);
}
void out_d(){
    struct Chat* p_chat;
    struct User* p_user;
    int i;

    //lascio ed elimino tutti i gruppi
    leaveGroup_me(NULL);

    //elimino tutte le chat private
    while(myChats){
        p_chat = myChats;
        myChats = myChats->next;
        deleteChat(p_chat);
    }

    if(isServerOnline)
        buf8 = 8;   sendNumber(sd_server, &buf8, 8);                    //invio messaggio di uscita al server

    //chiudo tutti i sd
    for(i=0; i<=fdmax; i++) {
        if(FD_ISSET(i, &master))
            close(i);
    }
    FD_ZERO(&master);

    //cancello la rubrica temporanea caricata in memoria
    while(addressBook){
        p_user = addressBook;
        addressBook = addressBook->next;
        free(p_user);
    }
}

///----------FUNZIONI AUSILIARIE (implementazione)---------
int newListenerSocket(){
    struct sockaddr_in my_addr;
    int my_sd, yes=1;

    my_sd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(my_sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));     // libera subito la porta al termine del processo
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(my_port);
    inet_pton(AF_INET, "localhost", &my_addr.sin_addr);    
    ret = bind(my_sd, (struct sockaddr*) &my_addr, sizeof(my_addr));
    if(ret<0){ perror("ERRORE BIND"); exit(-1); }
    ret = listen(my_sd, 10);

    return my_sd;
}
int newCommunicationSocket(){
    int addrlen, newfd;
    struct sockaddr_in otherDevice_addr;

    addrlen = sizeof(otherDevice_addr);
    newfd = accept(listener, (struct sockaddr *) &otherDevice_addr, (socklen_t*)&addrlen);

    FD_SET(newfd, &master);
    if(newfd > fdmax)
        fdmax = newfd; // Aggiorno fdmax
    
    return newfd;
}
int connect_to_a_server(uint16_t port){                                 //funzione che stabilisce la connessione con un server
    struct sockaddr_in server_addr;
    int ret, sd;
    
    sd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));                       // pulizia
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, "localhost", &server_addr.sin_addr);
    ret = connect(sd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if(ret < 0){ perror("> ERRORE in fase di connessione"); puts("> il server potrebbe essere momentaneamente offline, riprova più tardi"); exit(-1); }

    FD_SET(sd, &master);
    if(sd > fdmax)
        fdmax = sd; // Aggiorno fdmax

    return sd;
}
void disconnect_from_a_server(int* sd){                                 //funzione che chiude la connnessione TCP con un server
    close(*sd);
    *sd = -1;
}

void readAddressBook(){                                                 //funzione che legge i contatti i rubrica e li carica in una struttura in memoria
    FILE* fp;
    char* res;
    char pathFile[37];
    struct User* tmp_user = NULL;
    sprintf(pathFile, "addressBooks/%s.txt", myusername);

    /* apre il file rubrica dell'utente */
    fp=fopen(pathFile, "r");
    if(fp==NULL){ 
        printf("> ATTENZIONE rubrica di %s non trovata.\n> Premi INVIO per proseguire\n", myusername);
        getchar();
        return;
    }

    /* legge la rubrica e riempie una struttura dati */
    while(1) {
        res=fgets(buffer, BUFFER_SIZE, fp);
        if(res==NULL)
            break;

        tmp_user = (struct User*) malloc(sizeof(struct User));  //qui la newUser non la posso usare
        //interpretazione riga
        sscanf(buffer, "%s", tmp_user->username);
        tmp_user->online = false;
        tmp_user->sd_chat = -1;
        tmp_user->next = NULL;
        pushUser(&addressBook, tmp_user);
    }
    fclose(fp);
}
void login(){
    char caption_1[] = "************************************ LOGIN *************************************\n\tDigita uno dei seguenti comandi:\n\n1) signup <srv_port> <username> <password>   → Crea un nuovo account\n2) in <srv_port> <username> <password>       → Esegui l'accesso al tuo account\n********************************************************************************\n";
    bool a,b, flag = false;
  
    puts(caption_1);
    do{
        scanf("%s", &buffer[0]);
        a = (strcmp(buffer, "signup") == 0);
        b = (strcmp(buffer, "in") == 0);

        if(!isServerOnline){
            printf("> purtroppo il server è offline → non si può iniziare una NUOVA chat\n> Premi INVIO per proseguire\n");
            return;
        }

        if(!(a||b)){
            flag=true;
            puts("> comando non disponibile, riprova\n");
        }
        else{                                                           //l'utente sta facendo "in" o "signup"
            fgets(buffer, BUFFER_SIZE, stdin);                          //legge un'itera riga dallo stdin
            sscanf(buffer, " %hd %s %s", &srv_port, myusername, mypassword);

            if (strlen(myusername)>INFO_SIZE-1 || strlen(myusername)>INFO_SIZE-1){
                puts("> l'username o la password non possono avere più di 19 caratteri. Riprova.\n");
                flag=1;
                continue;
            }

            buf8= (a)? 0 : 1;                                           // 0 se signup, 1 se in
            sd_server = connect_to_a_server(srv_port);                  //mi connetto al server principale
            sendNumber(sd_server, &buf8, 8);

            if(a)       //signup
                flag = signup_d();
            else if(b)  //in
                flag = in_d();
        }
    }
    while (flag);                                                       //esco dal ciclo solo se viene fatta una "in"
}   

void showMenu(){
    char caption_0[] = "******************************** MENU INIZIALE *********************************";
    char caption_1[] = "\tDigita uno dei seguenti comandi:\n1) hanging            → Stampa utenti che hanno inviato msg mentre si è offline\n2) show <username>    → Stampa msg pendenti inviati da un utente\n3) chat <username>    → Entra in chat con un utente o con un gruppo\n4) share <file_name>  → Invia un file  [Da usare nella schermata chat]\n5) out                → Chiude l'applicazione\n********************************************************************************\n";    //fputs("\033c", stdout);

    //system("clear");                                                  //per debuggare meglio, decommentare questa riga e commentare quella sottostante
    fputs("\033c", stdout);
    puts(notification_1);
    puts(notification_2);
    puts(notification_3);
    //puts("");

    puts(caption_0);
    printf("> accesso effettuato come: %s\n\n", myusername);
    puts(caption_1);
}
void showChatInterface(struct Chat* p_chat){
    struct Msg* p_msg;
    struct Member* p_m;

    //system("clear");                                                  //per debuggare meglio, decommentare questa riga e commentare quella sottostante
    fputs("\033c", stdout);
    puts(notification_1);
    puts(notification_2);
    puts(notification_3);
    //puts("");

    printf("********************************************************************************\n");
    if(!p_chat->isGroup){
        printf("\t\t\t%s", p_chat->name_chat);

        if(p_chat->members->userPointer->online)
            printf(ANSI_COLOR_GREEN   "\t\tè online\n"   ANSI_COLOR_RESET);
        else if(!p_chat->members->userPointer->online)
            printf(ANSI_COLOR_RED   "\t\tè offline\n"   ANSI_COLOR_RESET);
    }
    else{
        printf(ANSI_COLOR_GREEN   "\t\t\t\t%s\n\t\t"   ANSI_COLOR_RESET, p_chat->name_chat);
        for(p_m=p_chat->members; p_m!=NULL; p_m=p_m->next)
            printf("%s, ", p_m->userPointer->username);
        printf("\b\b  \n");
    }

    printf("********************************************************************************\n\n");

    for(p_msg = p_chat->messages; p_msg != NULL ; p_msg = p_msg->next){
        if(p_msg->state == 0)
            printf("%s:\t%s\n", p_msg->name_source, p_msg->text);
        else if(p_msg->state == 1)
            printf("%s:  * %s\n", p_msg->name_source, p_msg->text);
        else
            printf("%s: ** %s\n", p_msg->name_source, p_msg->text);
    }
    puts("\n\n\n\n\n————————————————————————————————————————————————————————————————————————————————\n> Scrivi un messaggio:");
}
void markAsGiven(){                                                     //funzione che mi viene chiamata in risposta ad un msg del server
    struct Chat* p_chat;                                                //e mi dice che un destinatario ha scaricato dei msg che gli hoo inviato mentre era offline
    struct Msg* p_msg;

    recvInfo(sd_server);
    p_chat = findChat(buffer);
    if(!p_chat) return;

    for(p_msg=p_chat->messages; p_msg; p_msg=p_msg->next){
        if(p_msg->state == 1)
            p_msg->state = 2;
    }

    p_chat->members->userPointer->online = true;
    if(inChat && strcmp(inChatWith, p_chat->members->userPointer->username)==0)
        showChatInterface(p_chat);
}

void addMemberGroup(char* typed_name, struct Chat* p_chat){             //funzione che viene chiamata quando si vuole aggiungere un utente al gruppo
    struct User* p_toAdd = findUser(typed_name);
    struct User* p_chattingWith;
    struct Member* p_m;

    if(!p_toAdd){                                                       //controllo se il contatto digitato è nella mia rubrica
        printf("> l'utente \"%s\" non è presente nella tua rubrica\n\n", typed_name);
        return;
    }
    
    if(!isServerOnline){                                                //se il server è offline non si può eseguire questo comando
        printf("> purtroppo il server è offline → impossibile eseguire questo comando adesso\n> Premi INVIO per proseguire\n");
        getchar();
        showChatInterface(p_chat);
        return;
    }

    askIfOnline_d(p_toAdd);
    if(!p_toAdd->online){                                               //controllo se il contatto digitato è online
        printf("> l'utente \"%s\" è offline, non puoi aggiungerlo alla chat\n\n", typed_name);
        return;
    }

    if(!p_chat->isGroup){                                               //se devo creare un gruppo da una chat privata, il mio interlocutore deve essere online
        p_chattingWith = findUser(inChatWith);
        askIfOnline_d(p_chattingWith);

        if(!p_chattingWith->online){
            printf("> NON puoi creare un gruppo se l'utente \"%s\" è offline\n\n", p_chattingWith->username);
            return;
        }
    }
    
    for(p_m=p_chat->members; p_m!=NULL; p_m=p_m->next){                 //controllo se l'utente è già membro di questa chat
        if(strcmp(p_toAdd->username, p_m->userPointer->username)==0){
            printf("> l'utente \"%s\" fa già parte di questa chat\n\n", typed_name);
            return;
        }
    }

    if(!p_chat->isGroup && p_chat->members->userPointer->sd_chat==-1){  //se devo creare un gruppo, devo prima aver mandato almeno un messaggio al mio interlocutore
        if(!p_chat->messages)
            printf("> Per creare un gruppo con \"%s\" devi prima inviargli almeno un messaggio\n\n", p_chattingWith->username);
        else
            printf("> Per creare un gruppo con \"%s\" devi inviargli un messaggio mentre è online\n> (Quelli inviati mentre è offline non contano)\n\n", p_chattingWith->username);
        
        return;
    }

    //CONTROLLI PRELIMINARI FINITI
    //ora posso finalmente aggiungere il membro al gruppo

    if(!p_chat->isGroup && p_chat->numPart == 1){                       //chat privata diventa un gruppo
        printf("> Questa chat privata sta per diventare una chat di gruppo.\n> Quale nome vuoi dare al gruppo?\n");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strlen(buffer)-1] = '\0';

        if (strlen(buffer) > INFO_SIZE-1){
            puts("> il nome del gruppo non può avere più di 19 caratteri. Riprova.\n");
            return;
        }

        strncpy(p_chat->name_chat, buffer, strlen(buffer)+1);
        strcpy(inChatWith, p_chat->name_chat);
        p_chat->isGroup = true;
    }

    //invio al nuovo membro le info del gruppo
    if(p_toAdd->sd_chat == -1){                                         //se non la conosco, chiedo la porta listener del nuovo membro al server, dopodiché ci stabilisco una connessione così da portergli inviare le informazioni
        p_toAdd->sd_chat = connect_to_a_server(askPort_d(p_toAdd));
        sendInfo(p_toAdd->sd_chat, myusername);
        p_toAdd->online = true;
    }

    buf8=3; sendNumber(p_toAdd->sd_chat, &buf8, 8);                     //dico al dest che sta arrivando un invito al gruppo
    
    sendInfo(p_toAdd->sd_chat, p_chat->name_chat);
    sendInfo(p_toAdd->sd_chat, myusername);

    buf16=p_chat->numPart; sendNumber(p_toAdd->sd_chat, &buf16, 16);
    for(p_m=p_chat->members; p_m!=NULL; p_m=p_m->next)
        sendInfo(p_toAdd->sd_chat, p_m->userPointer->username);

    //inserisco a tutti gli effetti (in locale) il nuovo membro al gruppo
    p_chat->numPart++;
    pushNewMember(p_chat, p_toAdd);

    //comunico al gruppo l'aggiunta del nuovo membro
    for(p_m=p_chat->members->next; p_m!=NULL; p_m=p_m->next){           //il p_m=...->next è importante perchè dato che aggiungo il nuovo membro in testa, in questo modo lo salto
        buf8=2; sendNumber(p_m->userPointer->sd_chat, &buf8, 8);
        sendInfo(p_m->userPointer->sd_chat, p_chat->name_chat);
        sendInfo(p_m->userPointer->sd_chat, p_toAdd->username);
    }

    showChatInterface(p_chat);
}
void receiveAddMemberGroup(int sd_source){                              //funzione che aggiunge un utente ad un gruppo a cui faccio parte
    struct User* p_source = findUser_by_sd(sd_source);
    struct User* p_toAdd;
    struct Chat* p_chat;
    char groupName[20];
    char coloredGroupName[30];
    bool iWasInThisChat = false;

    recvInfo(sd_source);                                                //ricevo il nome del gruppo in questione
    strncpy(groupName, buffer, strlen(buffer)+1);

    //suorce mi invia i dati del nuovo mebro a prescindere che io li abbia già nella mia rubrica
    recvInfo(sd_source);
    p_toAdd = findUser(buffer);
    if(p_toAdd)                                                         //se già è presente nella mia rubrica (temporanea) mi segno solo che è online
        p_toAdd->online = true;
    else{                                                               //altrimenti lo aggiungo ora
        p_toAdd = newUser(buffer, true);
        pushUser(&addressBook, p_toAdd);
    }

    //valuto se la chat privata deve diventare di gruppo, oppure lo è già
    p_chat = findChat(groupName);
    if(p_chat){
        //questa chat era già un gruppo
        p_chat->numPart++;
        pushNewMember(p_chat, p_toAdd);

        sprintf(dummy_Notification, ">>! NOTIFICA: %s fa ora parte del gruppo \"%s\"", p_toAdd->username, groupName);
        newNotification();

        if(inChat && strcmp(inChatWith, groupName)==0)
            showChatInterface(p_chat);
    }
    else{
        //questa chat privata deve diventare un gruppo
        if(inChat && strcmp(inChatWith, p_source->username)==0)
            iWasInThisChat = true;

        p_chat = findChat(p_source->username);
        strcpy(p_chat->name_chat, groupName);
        p_chat->isGroup = true;
        p_chat->numPart++;
        pushNewMember(p_chat, p_toAdd);

        if(iWasInThisChat)
            strcpy(inChatWith, groupName);
        
        sprintf(coloredGroupName, ANSI_COLOR_GREEN  "%s"   ANSI_COLOR_RESET, groupName);
        sprintf(dummy_Notification, ">>! NOTIFICA: la chat con %s è ora una chat di gruppo di nome: \"%s\"", p_source->username, coloredGroupName);
        newNotification();
        sprintf(dummy_Notification, ">>! NOTIFICA: %s fa ora parte del gruppo \"%s\"", p_toAdd->username, groupName);
        newNotification();
    }
}
void receiveGroupInvite(int sd_source){                                 //ricevo l'invito a unirmi ad un gruppo, mi faccio quindi inviare tutti i dati necessari
    struct Chat* p_chat;
    struct User* p_toAdd, * p_source;
    char name_source[20];
    char groupName[20];
    char coloredGroupName[30];
    int i;

    recvInfo(sd_source);                                                //ricevo nome gruppo
    strncpy(groupName, buffer, strlen(buffer)+1);
    recvInfo(sd_source);                                                //ricevo nome mittente invito
    strncpy(name_source, buffer, strlen(buffer)+1);
    recvNumber(sd_source, 16);                                          //ricevo numero membri del gruppo

    p_source = findUser(name_source);
    p_chat = pushNewChat(p_source);
    strcpy(p_chat->name_chat, groupName);
    p_chat->isGroup = true;
            
    for(i=0; i < buf16; i++){                                           //ricorda: il source viene già aggiunto dalla pushNewChat
        recvInfo(sd_source);                                            //ricevo nome di un mebro del gruppo
        p_toAdd = findUser(buffer);

        if(p_toAdd)                                                     //se già è presente nella mia rubrica (temporanea) mi segno solo che è online
            p_toAdd->online = true;
        else{                                                           //altrimenti lo aggiungo ora
            p_toAdd = newUser(buffer, true);
            pushUser(&addressBook, p_toAdd);
        }

        pushNewMember(p_chat, p_toAdd);
        p_chat->numPart++;
    }

    strcpy(buffer, "\t<< join group >>");
    sendMsg(p_chat);

    sprintf(coloredGroupName, ANSI_COLOR_GREEN  "%s"   ANSI_COLOR_RESET, groupName);
    sprintf(dummy_Notification, ">>! NOTIFICA: %s ti ha aggiunto al gruppo \"%s\"", name_source, coloredGroupName);
    newNotification();
}

void leaveGroup_me(struct Chat* groupToDelete){
    struct Chat* p_chat;

    if(groupToDelete){
        //cancello solo il group indicato
        strcpy(buffer, "\t<< leaving group >>");
        sendMsg(groupToDelete);
        deleteChat(groupToDelete);
    }
    else{
        //se ho passato NULL come argomento, vuol dire che voglio uscire da TUTTI i gruppi
        do{
            p_chat = NULL;
            for(p_chat=myChats; p_chat && !p_chat->isGroup; p_chat=p_chat->next)    //scorro le mie chat in cerca di gruppi
                ;

            if(p_chat){
                strcpy(buffer, "\t<< leaving group >>");
                sendMsg(p_chat);
                deleteChat(p_chat);
            }
        }
        while(p_chat);                                                              //se tra le mie chat non ho più gruppi ho finito
    }
}
void deleteChat(struct Chat* victim){
    struct Chat* p_chat, *prev=NULL;
    struct Msg* p_msg;
    struct Member* p_m;

    if(!victim)
        exit(-1);

    //collego tra loro gli elementi della lista adiacenti a victim 
    for(p_chat=myChats; p_chat && p_chat!= victim; p_chat=p_chat->next)
        prev = p_chat;

    if(!p_chat || p_chat!=victim)
        exit(-1);
    if(p_chat==myChats)
        myChats = myChats->next;
    else
        prev->next = p_chat->next;

    //passo ora ad elimanare la chat
    while(victim->members){
        p_m = victim->members;
        victim->members = victim->members->next;
        free(p_m);
    }
    while(victim->messages){
        p_msg = victim->messages;
        victim->messages = victim->messages->next;
        free(p_msg->text);
        free(p_msg);
    }
    free(victim);
}

void removeMemberfromALLGroups(struct User* target){                    //elimino un utente da tutti i gruppi di cui faceva parte
    struct Chat* p_chat;
    struct Member* p_m;
    bool again = true;

    while(again){
        again = false;

        for(p_chat=myChats; p_chat; p_chat=p_chat->next){    //scorro le mie chat in cerca di gruppi
            if(p_chat->isGroup){
                for(p_m=p_chat->members; p_m; p_m=p_m->next){
                    if(strcmp(p_m->userPointer->username, target->username)==0){
                        removeMemberfromSpecificGroup(p_chat, target->username);
                        again = true;
                        break;
                    }
                }
            }
        }
    }
}
bool removeMemberfromSpecificGroup(struct Chat* p_chat, char* who){     //elimino un utente da uno specifico gruppo di cui faceva parte
    struct Member *p_m, *prev=NULL;

    //lo rimuovo dal gruppo
    for(p_m=p_chat->members; p_m && strcmp(p_m->userPointer->username, who)!=0; p_m=p_m->next)
        prev = p_m;

    if(!p_m || strcmp(p_m->userPointer->username, who)!=0)
        exit(-1);
    if(p_m == p_chat->members)
        p_chat->members = p_chat->members->next;
    else
        prev->next=p_m->next;

    sprintf(dummy_Notification, ">>! NOTIFICA: %s ha abbandonato il gruppo \"%s\"", p_m->userPointer->username, p_chat->name_chat);
    newNotification();

    p_chat->numPart--;    
    free(p_m);
    
    if(inChat && strcmp(inChatWith, p_chat->name_chat)==0)
        showChatInterface(p_chat);

    if(p_chat->numPart == 0){
        if(inChat && strcmp(inChatWith, p_chat->name_chat)==0){         //se eravamo rimasti in 2 nel gruppo, ora il gruppo deve essere sciolto e cancellato dalla memoria
            printf("> ATTENZIONE sei rimasto da solo qui. Il gruppo sta per essere eliminato.\n> Premi INVIO per proseguire\n");
            getchar();
        }
        else{
            sprintf(dummy_Notification, ">>! NOTIFICA: il gruppo \"%s\" è stato sciolto", p_chat->name_chat);
            newNotification();
        }

        deleteChat(p_chat);
        inChat = false;
        showMenu();
        return false;
    }
    else
        return true;
}

void listOnlineFriends(){                                               //funzione che stampa una lista dei miei contatti attualmente online
    struct User* p_user;
    struct Chat* p_chat = findChat(inChatWith);
    struct Member* check;
    bool alreadyIncluded;
    int counter = 0;

    if(!isServerOnline){                                                //se il server è offline non si può eseguire questo comando
        printf("> purtroppo il server è offline → impossibile eseguire questo comando adesso\n> Premi INVIO per proseguire\n");
        getchar();
        showChatInterface(p_chat);
        return;
    }

    for(p_user=addressBook; p_user!=NULL; p_user=p_user->next){
        askIfOnline_d(p_user);                                          //devo farlo anche per quelli che ho visto essere online perchè *se non ci ho mai parlato* non ho modo di sapere se sono andati offline

        if(p_user->online){                                             //controllo se l'utente è già membro di questa chat
            alreadyIncluded = false;
            for(check=p_chat->members; check!=NULL; check=check->next){
                if(strcmp(p_user->username, check->userPointer->username)==0){
                    alreadyIncluded = true;
                    break;
                }
            }

            if(!alreadyIncluded){
                counter++;
                if(counter==1)
                    printf("> Gli utenti attualmente online che puoi aggiungere a questa chat sono:\n  > ");
                printf("%s, ", p_user->username);
            }
        }
    }

    if(counter>0)
        printf("\b\b  \n\n");
    else
        printf("> Non hai amici attualmente online che puoi aggiungere a questa chat\n\n");
}
void askIfOnline_d(struct User* p_user){
    buf8 = 6;   sendNumber(sd_server, &buf8, 8);                        //chiedo al server una askIfOnline
    sendInfo(sd_server, p_user->username);                              //gli invio il nome del target
    recvNumber(sd_server, 8);                                           //ricevo il suo stato

    switch(buf8){
        case 0:
            p_user->online = false;
            break;
        case 1:
            p_user->online = true;
            break;
        case 2:
            printf("> ERRORE CRITICO: l'utente \"%s\" è presente nella tua rubrica, ma non nel database\n", p_user->username);
            exit(-1);
            break;
    }
}
uint16_t askPort_d(struct User* p_user){
    buf8 = 7;   sendNumber(sd_server, &buf8, 8);                        //invio al server il codice della ask_Port
    sendInfo(sd_server, p_user->username);                              //gli invio il nome del target
    recvNumber(sd_server, 16);                                          //ricevo la porta chiesta
    return buf16;
}
void newNotification(){
    //Questa funzione è stata pensata nel seguente modo:
    //se mi trovo nella chat con l'utente argomento della notifica, questa funzione
    //NON deve essere chiamata. È LA FUNZIONE CHIAMANTE CHE DEVE FARE QUESTO CONTROLLO

    strcpy(notification_1, notification_2);
    strcpy(notification_2, notification_3);
    strcpy(notification_3, dummy_Notification);

    if(inChat){    //con un utente che è argomento della notifica
        showChatInterface(findChat(inChatWith));        
    }
    else{
        showMenu();
    }
}
void clearNotification(){                                               //funzione che "ripulisce" le notifiche arrivate
    strcpy(notification_1, "");
    strcpy(notification_2, "");
    strcpy(notification_3, "");
}
void receiveFile(int sd_source){                                        //funzione che riceve i file in arrivo
    FILE* fp;
    char name_file[50];
    char name_source[20];
    char name_chat[20];
    char pathFile[77];

    recvInfo(sd_source);                                                //ricevo le info sul file, quali: nome della chat, nome del mittente e nome file
    strcpy(name_chat, buffer);
    recvInfo(sd_source);
    strcpy(name_source, buffer);
    recvText(sd_source);
    strncpy(name_file, buffer, 50);

    sprintf(pathFile, "files_%s/%s", myusername, name_file);
    fp=fopen(pathFile, "w");
    if(fp==NULL){ 
        printf("> ATTENZIONE impossibile creare nuovo file %s\n", name_file);
        return;
    }

    while(1){
        recvText(sd_source);                                            //ricevo segmenti del file in arrivo
        
        if(strcmp(buffer, "<< end-of-file >>")!=0)                      //leggo il segmento arrivato, se corrisponde ad "end-of-file" vuole dire che non devo più ricevere
            fprintf(fp,"%s", buffer);
        else{
            fclose(fp);
            break;
        }
    }
}

///----------FUNZIONI DI UTILITÀ (implementazione)---------
void sendMsg(struct Chat* p_chat){                                      //funzione che sta alla base dell'invio dei messaggi. In base al caso specifico, questa chiamerà delle altre funzioni affinchè il messaggio venga effetivamente inviato
    struct User* p_dest = p_chat->members->userPointer;
    struct Member* p_m;

//è un FirstMsg/anotherPendingMessage, devo quindi passare dal server
    if(!p_chat->isGroup && p_dest->sd_chat==-1){
        if(!isServerOnline){
            printf("> purtroppo il server è offline → non puoi inviare msg ad un utente OFFLINE\n> Premi INVIO per proseguire\n");
            getchar();
            showChatInterface(p_chat);
            return;
        }

        askIfOnline_d(p_dest);                  //richiedo al server se il dest è offline perchè non ho modo di sapere se è uscito prima che io inviassi un messaggio in quanto ancora non c'è una connessione tra i due

        //cerco se l'ultimo messaggio che gli ho inviato ha state==1
        if(isMyFirstOfflineMsg(p_chat)){
            sendFirstMsg(p_dest, p_chat);                                                   //invio il msg al server, e lui lo recapiterà al destinatario
            pushMsg(p_chat, newMsg(p_dest->username, "Tu", buffer, p_dest->online+1));      //inserisco il messaggio nella chat
        }
        else{
            sendMsg_to_server(p_dest);          //è offline, ma gli mando altri msg
            pushMsg(p_chat, newMsg(p_chat->members->userPointer->username, "Tu", buffer, 1));
        }
    }
//se ho già stabilito una connessione con il destinatario, invio io stesso il messaggio (senza passare dal server)
    else if(!p_chat->isGroup && p_dest->sd_chat!=-1){
        buf8=1; sendNumber(p_dest->sd_chat, &buf8, 8);                                      //dico all'altro device che sta per arrivare un msg
        sendInfo(p_dest->sd_chat, p_chat->name_chat);
        sendInfo(p_dest->sd_chat, myusername);
        sendText(p_dest->sd_chat);
        pushMsg(p_chat, newMsg(p_chat->name_chat, "Tu", buffer, 2));
    }
//invio a gruppo
    else if(p_chat->isGroup){
        for(p_m=p_chat->members; p_m!=NULL; p_m=p_m->next){
            p_dest = p_m->userPointer;
            if(p_dest->sd_chat==-1){                                                        //se non ho mai parlato con un membro non ho una connessione con lui, quindi devo passare dal server
                sendFirstMsg(p_dest, p_chat);
            }
            else{       //se conosco l'sd lo mando io
                buf8=1; sendNumber(p_dest->sd_chat, &buf8, 8);                              //dico all'altro device che sta per arrivare un msg
                sendInfo(p_dest->sd_chat, p_chat->name_chat);
                sendInfo(p_dest->sd_chat, myusername);
                sendText(p_dest->sd_chat);
            }
        }
        
        pushMsg(p_chat, newMsg(p_chat->name_chat, "Tu", buffer, 2));
    }
    else{
        puts("> ERRORE CRITICO: caso non gestito!");
    }
}
void sendFirstMsg(struct User* p_dest,  struct Chat* p_chat){
    buf8 = 2;   sendNumber(sd_server, &buf8, 8);                        //dico al server che voglio inviare un PRIMO msg
    sendInfo(sd_server, p_dest->username);                              //comunico il destinatario del messagio

    recvNumber(sd_server, 8);                                           //controllo che l'utente sia presente nel database
    if(buf8 == 0){ printf("> ERRORE CRITICO: l'utente \"%s\" è presente nella tua rubrica, ma non nel database\n", p_dest->username); exit(-1); }

    sendInfo(sd_server, p_chat->name_chat);                             //dico a quale chat appartiene il messaggio
    sendText(sd_server);                                                //invio il testo del messaggio
    recvNumber(sd_server, 8);                                           //NON TOGLIERE: ricevo l'esito dell'invio del messaggio (se online o no)
    switch(buf8){
        case 1:     // DEST online 
            recvNumber(sd_server, 16);                                  //ricevo la porta del dest
            p_dest->sd_chat = connect_to_a_server(buf16);               //faccio una connect al listener del destinatario
            p_dest->online = true;
            break;
        case 2:     // DEST offline -> msg memorizzato sul server
            p_dest->online = false;
    }
}
void sendMsg_to_server(struct User* p_dest){                            //serve per inviare messaggi ad utente che è attualmente online
    struct Chat* p_chat = findChat(p_dest->username);

    if(p_chat->isGroup){
        puts("NON PUOI INVIARE UN MESSAGGIO AD UN GRUPPO TRAMITE IL SERVER");
        return;
    }

    buf8 = 3;   sendNumber(sd_server, &buf8, 8);                        //dico al server che voglio inviare un "otherMsg"
    sendInfo(sd_server, p_chat->members->userPointer->username);
    sendInfo(sd_server, myusername);
    sendText(sd_server);   
}
bool isMyFirstOfflineMsg(struct Chat* p_chat){
    struct Msg* p_msg, * target = NULL;
    
    if(!p_chat->messages)
        return true;

    for(p_msg=p_chat->messages; p_msg; p_msg=p_msg->next){
        if(strcmp(p_msg->name_source, myusername)==0)
            target = p_msg;
    }
    
    if(!target || target->state == 2)
        return true;
    else
        return false;
}

bool recvMsg(int sd_source){                                            //funzione che sta alla base della ricezione dei messaggi
    //assumo che la chat con source sia già stata creata
    struct Chat* p_chat;
    struct Msg* p_msg;
    char name_chat[INFO_SIZE];
    char name_source[INFO_SIZE];

    recvInfo(sd_source);                                                //ricevo nome chat
    strncpy(name_chat, buffer, strlen(buffer)+1);
    recvInfo(sd_source);                                                //ricevo nome mittente
    strncpy(name_source, buffer, strlen(buffer)+1);
    recvText(sd_source);                                                //ricevo testo del messaggio

    p_chat = findChat(name_chat);                                       //se il msg è di una chat privata qui avrò NULL
    if(!p_chat){
        p_chat = findChat(name_source);

        if(!p_chat)                                                     //se p_chat è ancora NULL è perchè non ho mai chattato con quest'utente
            p_chat = pushNewChat(findUser(name_source));
    }
    
    p_msg = newMsg(name_chat, name_source, buffer, 2);
    pushMsg(p_chat, p_msg);                                             //carico il messaggio nella chat
    
    if(strncmp(buffer, "\t<< ", 4)!=0){                                 //se il text inizia con quel pattern non devo stampare il messagio
        if(strcmp(name_chat, myusername)==0){                           //msg chat privata

            if(inChat && strcmp(inChatWith, name_source)==0){           //se mi trovo nella chat allora stampo messaggio, altrimenti mostro notifica
                //mi basta che il msg sia stato inserito in lista
                showChatInterface(p_chat);
            }
            else{
                sprintf(dummy_Notification, ">>! NOTIFICA: c'è un nuovo msg da %s", name_source);
                newNotification();
            }
        }
        else{                                                       //msg chat gruppo
            if(inChat && strcmp(inChatWith, name_chat)==0){
                //mi basta che il msg sia stato inserito in lista
                showChatInterface(p_chat);
            }
            else{
                sprintf(dummy_Notification, ">>! NOTIFICA: c'è un nuovo msg sul gruppo %s", name_chat);
                newNotification();
            }
        }
    }
    else{
        if(inChat && strcmp(inChatWith, name_chat)==0)
            showChatInterface(p_chat);
    }
    
    if(p_chat->isGroup && strcmp(p_msg->text, "\t<< leaving group >>") == 0)
        return removeMemberfromSpecificGroup(p_chat, p_msg->name_source);
    else
        return true;
}
bool receiveFirstMsg(){
    /* il server mi recapita un primo messaggio e mi dice chi me l'ha mandato, memorizzo il messaggio recapitato
       e mi preparo a ricevere una connect dall'utente che mi ha mandato un msg attraverso il server */
    struct User* p_source;
    struct Chat* p_chat;
    struct Msg* p_msg;
    char name_chat[INFO_SIZE];
    bool forGroup = true;

    recvInfo(sd_server);                                                //ricevo nome mittente
    p_source = findUser(buffer);
    if(!p_source){
        p_source = newUser(buffer, true);
        pushUser(&addressBook, p_source);
    }

    recvInfo(sd_server);                                                //ricevo nome chat
    strncpy(name_chat, buffer, strlen(buffer)+1);
    recvText(sd_server);                                                //ricevo testo messaggio

    p_chat = findChat(name_chat);
    if(!p_chat){                                                        //qui capisco se il msg è privato o di gruppo
        p_chat = findChat(p_source->username);

        if(!p_chat)                                                     //se p_chat è ancora NULL è perchè non ho ancora chattato con quest'utente
            p_chat = pushNewChat(p_source);
        
        strcpy(name_chat, p_source->username);
        forGroup = false;
    }

    if(inChat && strcmp(inChatWith, name_chat)==0){                     //se mi trovo nella chat allora stampo messaggio, altrimenti mostro notifica
            //mi basta che il msg sia stato inserito in lista
            showChatInterface(p_chat);
    }
    else if(strncmp(buffer, "\t<< ", 4)!=0){
        if(!p_chat->isGroup)
            sprintf(dummy_Notification, ">>! NOTIFICA: c'è un nuovo msg da %s", name_chat);
        else
            sprintf(dummy_Notification, ">>! NOTIFICA: c'è un nuovo msg sul gruppo %s", name_chat);
        
        newNotification();
    }

    if(forGroup){                                                       //a seconda che il messaggio sia di gruppo o privato va salvato in due modi diversi
        p_msg = newMsg(name_chat, p_source->username, buffer, 2);
        pushMsg(p_chat, p_msg);
    }
    else{
        p_msg = newMsg(myusername, p_source->username, buffer, 2);
        pushMsg(p_chat, p_msg);
    }

    p_source->sd_chat = newCommunicationSocket();                       //faccio una accept in attesa che il mittente mi ffaccia una connect
    p_source->online = true;

    if(inChat && strcmp(inChatWith, name_chat)==0)
        showChatInterface(p_chat);

    if(p_chat->isGroup && strcmp(p_msg->text, "\t<< leaving group >>") == 0)
        return removeMemberfromSpecificGroup(p_chat, p_msg->name_source);
    else
        return true;
}
void sendText(int sd_dest){                                             //funzione che invia una stringa di testo di lunghezza < BUFFER_SIZE
    uint16_t lmsg;

    lmsg = strlen(buffer)+1;
    sendNumber(sd_dest, &lmsg, 16);

    ret = send(sd_dest, (void*)buffer, lmsg, 0);
    if(ret < lmsg){ perror("Errore nella \"send\""); exit(-1); }
}
void recvText(int sd_source){                                           //funzione che riceve una stringa di testo di lunghezza < BUFFER_SIZE
    recvNumber(sd_source, 16);
    
    ret = recv(sd_source, (void*)buffer, buf16, 0);
    if(ret < buf16){ perror("Errore nella \"receive\""); exit(-1); }
}
void sendInfo(int sd_dest, char* info){                                 //funzione che invia una stringa di testo di lunghezza < INFO_SIZE
    ret = send(sd_dest, (void*)info, INFO_SIZE, 0);
    if(ret < INFO_SIZE){ perror("Errore nella \"send\""); exit(-1); }
}
void recvInfo(int sd_source){                                           //funzione che riceve una stringa di testo di lunghezza < INFO_SIZE
    ret = recv(sd_source, (void*)buffer, INFO_SIZE, 0);
    if(ret < INFO_SIZE){ perror("Errore nella \"receive\""); exit(-1); }
    //LA STRINGA CERCATA É ORA IN "buffer"
}
void sendNumber(int sd_dest, void* pun, uint8_t size){                  //funzione che invia un numero di 8 o 16 bit
    uint16_t dummy16;

    if(size == 8){
        ret = send(sd_dest, (void*)pun, sizeof(uint8_t), 0);
        if(ret < sizeof(uint8_t)){ perror("Errore nella \"send\""); exit(-1); }
    }
    else if(size == 16){
        dummy16 = *((uint16_t*)pun);
        dummy16 = htons(dummy16);

        ret = send(sd_dest, (void*)&dummy16, sizeof(uint16_t), 0);
        if(ret < sizeof(uint16_t)){ perror("Errore nella \"send\""); exit(-1); }
    }
}
void recvNumber(int sd_source, uint8_t size){                           //funzione che riceve un numero di 8 o 16 bit
    if(size == 8){
        ret = recv(sd_source, (void*)&buf8, sizeof(uint8_t), 0);
        if(ret < sizeof(uint8_t)){ perror("Errore nella \"receive\""); exit(-1); }
        //il numero carcato É ORA IN "buf8"
    }
    else if(size == 16){
        ret = recv(sd_source, (void*)&buf16, sizeof(uint16_t), 0);
        if(ret < sizeof(uint16_t)){ perror("Errore nella \"receive\""); exit(-1); }
        buf16 = ntohs(buf16);
        //il numero carcato É ORA già convertito IN "buf16"
    }
}

struct User* newUser(char* newUsername, bool newOnline){                //funzione che permette di memorizzare un nuovo contatto
    struct User* tmp_user = NULL;
    tmp_user = (struct User*) malloc(sizeof(struct User));

    strncpy (tmp_user->username, newUsername, strlen(newUsername)+1);
    tmp_user->online = newOnline;
    tmp_user->sd_chat = -1;
    tmp_user->next = NULL;

    return tmp_user;
}
struct Msg* newMsg(char* newName_dest, char* newName_source, char* newText, uint8_t newState){  //funzione che permette di memorizzare un nuovo msg
    struct Msg* tmp_msg;
    uint16_t len = (strlen(newText))+1;
    tmp_msg = (struct Msg*) malloc(sizeof(struct Msg));
    tmp_msg->text = (char*) malloc(len);

    strncpy (tmp_msg->name_dest, newName_dest, strlen(newName_dest)+1);
    strncpy (tmp_msg->name_source, newName_source, strlen(newName_source)+1);
    strncpy (tmp_msg->text, newText, len);
    tmp_msg->state = newState;
    tmp_msg->next = NULL;

    return tmp_msg;
}
struct Chat* pushNewChat(struct User* firstParticipant){                //funzione che permette di memorizzare una nuova chat
    struct Chat* p_chat = NULL;
    p_chat = (struct Chat*) malloc(sizeof(struct Chat));
    
    p_chat->isGroup = false;
    strcpy(p_chat->name_chat, firstParticipant->username);
    p_chat->numPart = 1;
    pushNewMember(p_chat, firstParticipant);
    p_chat->messages = NULL;
    p_chat->next = NULL;

    //inserisco la nuova chat nella lista delle chat
    p_chat->next = myChats;
    myChats = p_chat;

    return p_chat;
}
void pushNewMember(struct Chat* p_chat, struct User* participant){      //funzione che permette di memorizzare un nuovo membro di un gruppo
    struct Member* p_mem = (struct Member*) malloc(sizeof(struct Member));
    p_mem->userPointer = participant;

    p_mem->next = p_chat->members;
    p_chat->members = p_mem;
}

void pushUser(struct User** head, struct User* new_data){               //funzione che inserisce una struct User in lista (inserimento in testa)
    new_data->next = *head;
    *head = new_data;
}
void pushMsg(struct Chat* chat, struct Msg* new_msg){                   //funzione che inserisce una struct Msg in lista (inserimento in coda)
    struct Msg* p;

    if(strncmp(buffer, "\t<< ", 4)==0)
        new_msg->state = 0;

    if(chat->messages == NULL){
        chat->messages = new_msg;
        return;
    }

    for(p=chat->messages; p->next != NULL; p=p->next)
        ;
    p->next = new_msg;
}

struct User* findUser(char* searched_username){                         //funzione che cerca un contatto nella rubrica a partire dal suo username
    struct User* p;
    if(!addressBook) return NULL;

    for(p=addressBook; (p!=NULL) && (strcmp(p->username, searched_username)!=0); p=p->next)
        ;
    
    return p;
}
struct User* findUser_by_sd(int searched_sd){                           //funzione che cerca un contatto nella rubrica a partire dal suo sd
    struct User* p;
    if(!addressBook) return NULL;

    for(p=addressBook; (p!=NULL) && (p->sd_chat != searched_sd); p=p->next)
        ;
    
    return p;
}
struct Chat* findChat(char* searched_username){                         //funzione che cerca una chat nelle "mie chats"
    struct Chat* p = myChats;
    if(!myChats) return NULL;

    for(p=myChats; (p!=NULL) && (strcmp(p->name_chat, searched_username)!=0); p=p->next) {
        ;
    }
    
    return p;
}

void enterInChatMode(struct Chat* p_chat){
    char username_newMember[INFO_SIZE+1];                               //il +1 perchè lo prendo con fgets (che mette un '\n')
    char name_file[50];
    struct User *p_user, tmp_user;
    int i;
    fd_set read_fds;

    inChat = true;
    strcpy(inChatWith, p_chat->name_chat);

    if(!p_chat->isGroup && p_chat->members->userPointer->sd_chat==-1)
        askIfOnline_d(p_chat->members->userPointer);                    //se non ho stabilito una connessione con l'interlocutore, chiedo al server se è online

    if(!p_chat->isGroup && isServerOnline)
        show_d(p_chat->name_chat);

    clearNotification();
    showChatInterface(p_chat);

    for(;;){
        read_fds = master;
        ret = select(fdmax +1, &read_fds, NULL, NULL, NULL);
        if(ret<0){ perror("ERRORE SELECT"); exit(-1); }

        for(i=0; i<=fdmax; i++) {
            if(!FD_ISSET(i, &read_fds))
                continue;
//STANDARD INPUT:
            if(i == 0){
                fgets(buffer, BUFFER_SIZE, stdin);                      //legge un'itera riga dallo stdin
                buffer[strlen(buffer)-1] = '\0';                        //perchè strlen non conta il '\0', ma conta il '\n' messo da fgets

                if(strncmp(buffer, "\\q", 2) == 0){
                    inChat = false;
                    showMenu();
                    return;
                }
                else if(strncmp(buffer, "\\u", 3) == 0){
                    listOnlineFriends();
                }
                else if(strncmp(buffer, "\\a ", 3) == 0){
                    memcpy(username_newMember, &buffer[3], 20);            
                    addMemberGroup(username_newMember, findChat(inChatWith));
                }
                else if(strncmp(buffer, "\\exit", 5) == 0){
                    if(!p_chat->isGroup){
                        puts("> comando \"exit\" non valido in una chat NON di gruppo\n");
                    }
                    else{
                        leaveGroup_me(p_chat);
                        inChat = false;
                        showMenu();
                        return;
                    }
                }
                else if(strncmp(buffer, "share ", 6) == 0){
                    memcpy(name_file, &buffer[6], 50);
                    share(p_chat, name_file);
                }
                else if(strcmp(buffer, "") == 0){
                    printf("\033[A\r\33[2K");
                }
                else{
                    sendMsg(p_chat);
                    showChatInterface(findChat(inChatWith));
                }
            }
//SOCKET ASCOLTO
            else if(i == listener){
                tmp_user.sd_chat = newCommunicationSocket();
                recvInfo(tmp_user.sd_chat);
                strncpy(tmp_user.username, buffer, strlen(buffer)+1);

                p_user = findUser(tmp_user.username);
                if(p_user){                                             //la richiesta al listener può arrivare solo da un mio contatto
                    p_user->online = true;
                    p_user->sd_chat = tmp_user.sd_chat;
                }
                else{                                                   //richiesta arrivata da un contatto NON in rubrica, lo aggiungo alla rubrica TEMPORANEA
                    pushUser(&addressBook, newUser(tmp_user.username, true));
                }
            }
//SERVER PRINCIPALE
            else if(i == sd_server){
                ret = recv(sd_server, (void*) &buf8, sizeof(uint8_t), 0);

                if(ret == 0){
                    sprintf(dummy_Notification, ">>! NOTIFICA: il SERVER è andato offline.");
                    newNotification();
                    isServerOnline = false;
                    close(i); FD_CLR(i, &master);
                }
                else if(ret < 0){
                    perror("ERRORE!");
                    close(i); FD_CLR(i, &master);
                }
                else{
                    switch(buf8){
                        case 1:     //receiveFirstMsg
                            if(!receiveFirstMsg())
                                return;
                            break;
                        case 2:     //il server mi dice che un utente ha fatto la show di alcuni miei messaggi pendenti
                            markAsGiven();
                            break;
                    }
                }
            }
//SOCKET COMUNICAZIONE            
            else {
                ret = recv(i, (void*) &buf8, sizeof(uint8_t), 0);

                if(ret == 0){                                           //rilevata chiusura di un client
                    p_user = findUser_by_sd(i);
                    p_user->online = false;
                    p_user->sd_chat = -1;

                    if(inChat && !p_chat->isGroup && strcmp(inChatWith, p_user->username)==0){
                        sprintf(dummy_Notification, ">>! NOTIFICA: l'utente %s è andato offline.", p_user->username);
                        newNotification();
                    }
                    removeMemberfromALLGroups(p_user);

                    close(i); FD_CLR(i, &master);
                }
                else if(ret < 0){
                    perror("ERRORE!");
                    close(i); FD_CLR(i, &master);
                }
                else{                                                   //un device mi ha inviato un msg
                    switch(buf8){
                        case 1:     //messaggio in arrivo
                            if(!recvMsg(i))
                                return;
                            break;
                        case 2:     //addMember in arrivo
                            receiveAddMemberGroup(i);
                            break;
                        case 3:     //group invite in arrivo
                            receiveGroupInvite(i);
                            break;
                        case 4:     //file in arrivo
                            receiveFile(i);
                            break;
                    }
                }
            }
        }
    } // Fine for(;;)
}