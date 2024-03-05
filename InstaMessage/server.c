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

typedef uint8_t bool;
#define true 1
#define false 0

#define BUFFER_SIZE 1024
#define INFO_SIZE 20
#define FIRST_MSG_SIZE 6

///---------------VARIABILI GLOBALI--------------------
struct UserInfo* database = NULL;
struct Mailbox* mailboxes = NULL;
struct HangingBox * hangingBoxes = NULL; 
int ret;
uint8_t buf8;
uint16_t srv_port, buf16;
uint32_t buf32;
char buffer[BUFFER_SIZE];

fd_set master;
int listener, fdmax;

///---------------STRUCT-------------------------------
struct UserInfo{
    char username[INFO_SIZE];
    char password[INFO_SIZE];
    uint16_t listener_port;
    int socket_s;
    time_t ts_login;
    time_t ts_logout;
    struct UserInfo* next;
};
struct PendingMsg{
    char username_source[INFO_SIZE];
    time_t ts;
    char* text;
    struct PendingMsg* next;
};
struct Mailbox{
    char username_dest[INFO_SIZE];
    struct PendingMsg* listMsg;
    struct Mailbox* next;
};
struct HangingBox{
    char username_source[INFO_SIZE];
    uint16_t msgCounter;
    time_t lastTS;
    struct HangingBox* next;
};


///----------FUNZIONI PRINCIPALI (dichiarazione)-------
void help();
void list();
void esc();
void signup_s(int);
void in_s(int);
void chat_firstMsg_s(int);
void chat_anotherPendingMsg_s(int);
void hanging_s(int);
void show_s(int);
void out_s(int);

///----------FUNZIONI AUSILIARIE (dichiarazione)-------
int newListenerSocket();
void newCommunicationSocket();
void readDatabase();
void writeNewRowDatabase(struct UserInfo*);
void askIfOnline_s(int);
void askPort_s(int);
bool isOnline(struct UserInfo*);

///----------FUNZIONI DI UTILITÀ (dichiarazione)-------
void sendText(int);
void recvText(int);
void sendInfo(int, char*);
void recvInfo(int);
void sendNumber(int, void*, uint8_t);
void recvNumber(int, uint8_t);
struct UserInfo* newUserInfo(char*, char*);
void pushUserInfo(struct UserInfo**, struct UserInfo*);
void pushNewPendingMsg(char*, char*, char*);
struct UserInfo* findUserInfo(char*);
struct UserInfo* findUserInfo_bySocketChat(int);
struct Mailbox* findMailbox(char*);
struct HangingBox* findHangingBox(char*);



int main(int argc, char* argv[]){
    char caption_0[] = "******************************** SERVER STARTED ********************************\n\tDigita uno dei seguenti comandi:\n1) help  → mostra i dettagli dei comandi\n2) list  → mostra un elenco degli utenti connessi\n3) esc   → chiude l'applicazione arrestando il server\n********************************************************************************\n";
    uint8_t type;

    int i;
    fd_set read_fds;

    //-------------INIZIO-------------------
    srv_port = (argc>1? atoi(argv[1]) : 4242);
    
    readDatabase();
    puts(caption_0);

    listener = newListenerSocket();
    // Reset dei descrittori
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_SET(0, &master);                 //descrittore dello standard input
    FD_SET(listener, &master);
    fdmax = listener;
    
    for(;;){
        read_fds = master;
        ret = select(fdmax +1, &read_fds, NULL, NULL, NULL);
        if(ret<0){ perror("ERRORE SELECT"); exit(-1); }

        for(i=0; i<=fdmax; i++) {
            if(!FD_ISSET(i, &read_fds))
                continue;
//STANDARD INPUT:
            if(i == 0){
                scanf("%s", &buffer[0]);

                if( strcmp(buffer, "help") == 0 )
                    help();
                else if( strcmp(buffer, "list") == 0 )
                    list();
                else if( strcmp(buffer, "esc") == 0 ){
                    esc();                 
                    return 0;       
                    }
                else
                    puts("> il comando digitato NON ESISTE, ritenta");
            }
//SOCKET ASCOLTO
            else if(i == listener){
                newCommunicationSocket();
            }
//SOCKET COMUNICAZIONE 
            else {
                ret = recv(i, (void*) &type, sizeof(uint8_t), 0);
                
                if(ret == 0){       //rilevata chiusura connessione con client
                    out_s(i);
                }
                else if(ret < 0){
                    perror("ERRORE!");
                    close(i); FD_CLR(i, &master);
                }
                else{
                    switch(type){
                        case 0: //signup
                            signup_s(i);
                            break;
                        case 1: //in
                            in_s(i);
                            break;
                        case 2: //chat (primo messaggio)
                            chat_firstMsg_s(i);
                            break;
                        case 3:
                            chat_anotherPendingMsg_s(i);
                            break;
                        case 4: //hanging
                            hanging_s(i);
                            break;
                        case 5: //show
                            show_s(i);
                            break;
                        case 6: //askIfOnline
                            askIfOnline_s(i);
                            break;
                        case 7: //askPort
                            askPort_s(i);
                            break;
                        case 8: //out
                            out_s(i);
                            break;
                    }
                }
            }
        }
    } // Fine for(;;)
}   ///-----FINE MAIN-----

 

///----------FUNZIONI PRINCIPALI (implementazione)---------
void help(){                                                            //funzione che mostra una breve descrizione dei comandi
    puts("> comando:\tlist\nMostra l'elenco dei soli utenti attualmente online, stampando per ognuno di essi “username*timestamp*porta”.\n\n> comando:\tesc\nChiude l'applicazione arrestando il server. I device non potranno comunicare con il server ma possono continuare le chat in corso.\n");
}
void list(){                                                            //funzione che stampa l'elenco degli utenti connessi alla rete
    struct tm *dateConverter;
    struct UserInfo* p_user;
    bool atLeastOne = false;    

    for(p_user=database; p_user; p_user=p_user->next){                  //scorro la lista degli utenti
        if(isOnline(p_user)){                                           //controllo se si è online guardando il timestamp di logout
            if(!atLeastOne)
                atLeastOne = true;

            dateConverter = localtime(&p_user->ts_login);
            strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", dateConverter);
            printf("\t%s*%s*%hd\n", p_user->username, buffer, p_user->listener_port);
        }
    }

    if(!atLeastOne)
        puts("> Non ci sono utenti attualmente online");
    puts("");
}
void esc(){                                                             //funzione che chiude l'applicazione server
    struct UserInfo* p_user;
    int i;
    FILE* fp;
    
    //segno sul log gli utenti attualmente online
    fp = fopen("log.txt", "a");
    if(fp==NULL){ perror("Errore in apertura del file"); exit(-1); }
    for(p_user=database; p_user; p_user=p_user->next){
        if(isOnline(p_user))
            fprintf(fp,"\n%s %ld %d", p_user->username, p_user->ts_login, 0);
    }                        
    fclose(fp);

    //chiudo tutti i socket aperti
    close(listener);
    for(i=0; i<=fdmax; i++){
        if(FD_ISSET(i, &master))
            close(i);
    }
    FD_ZERO(&master);                                                    //Termina il server

    //cancello il database temporaneo nell'heap
    while(database){
        p_user = database;
        database = database->next;
        free(p_user);
    }
}
void signup_s(int sd_source){                                           //funzione lato server del comando signup
    char typed_username[INFO_SIZE];
    char typed_password[INFO_SIZE];
    struct UserInfo* tmp_punt;

    recvInfo(sd_source);                                                //ricevo typed_username e typed_password
    strncpy(typed_username, buffer, strlen(buffer)+1);
    recvInfo(sd_source);
    strncpy(typed_password, buffer, strlen(buffer)+1);

    if(findUserInfo(typed_username) != NULL){                           //controllo se username è già usato
        buf8=0; sendNumber(sd_source, &buf8, 8);
    }
    else{                                                               //se username risulta inutilizzato
        tmp_punt = newUserInfo(typed_username, typed_password);
        writeNewRowDatabase(tmp_punt);    
        pushUserInfo(&database, tmp_punt);
        buf8=1; sendNumber(sd_source, &buf8, 8);
    }

    printf("> %s si è appena registrato\n", typed_username);
    
    close(sd_source);
    FD_CLR(sd_source, &master);
}
void in_s(int sd_source){                                               //funzione lato server del comando in
    char typed_username[INFO_SIZE];
    char typed_password[INFO_SIZE];
    struct UserInfo* p;

    recvInfo(sd_source);                                                //ricevo typed_username e typed_password
    strncpy(typed_username, buffer, strlen(buffer)+1);
    recvInfo(sd_source);
    strncpy(typed_password, buffer, strlen(buffer)+1);

    p=findUserInfo(typed_username);
    if(p==NULL){                                                        //non esiste nel database
        buf8=0; sendNumber(sd_source, &buf8, 8);
    }
    else if(strcmp(p->password, typed_password)!=0){                    //password errata
        buf8=1; sendNumber(sd_source, &buf8, 8);
    }
    else{                                                               //password corretta e p punta il UserInfo giusto
        buf8=2; sendNumber(sd_source, &buf8, 8);                        //dico al client che login OK, così mi invia la sua porta
        recvNumber(sd_source, 16);                                      //ricevo porta ascolto dell'utente
        p->listener_port = buf16;
        p->socket_s = sd_source;
        p->ts_login = time(NULL);
        p->ts_logout = 0;
        printf("> %s ha eseguito l'accesso\n", typed_username);
    }
}
void chat_firstMsg_s(int sd_source){                                    //funzione che permette ai device di inviare un msg ad un utente a cui non hanno ancora inviato un messaggio e di cui non conoscono la porta
    char username_dest[INFO_SIZE];
    char name_chat[INFO_SIZE];
    struct UserInfo* dest, * source;

    recvInfo(sd_source);                                                //ricevo username_dest
    strncpy(username_dest, buffer, strlen(buffer)+1);

    source = findUserInfo_bySocketChat(sd_source);
    dest = findUserInfo(username_dest);
    if(dest == NULL){                                                   //dest non presente in database
        buf8 = 0; sendNumber(sd_source, &buf8, 8);
        return;
    }
    else{
        buf8 = 1; sendNumber(sd_source, &buf8, 8);
    }

    recvInfo(sd_source);                                                //ricevo name_chat
    strncpy(name_chat, buffer, strlen(buffer)+1);

    recvText(sd_source);

    if(isOnline(dest)){
        buf8 = 1; sendNumber(dest->socket_s, &buf8, 8);                 //dico a DEST che sta per arrivare msg

        sendInfo(dest->socket_s, source->username);                     //invio al dest il mittente
        sendInfo(dest->socket_s, name_chat);                            //dico al dest a quale chat appartiene il msg
        sendText(dest->socket_s);                                       //invio msg al dest
        buf8 = 1; sendNumber(sd_source, &buf8, 8);                      //NON TOGLIERE: invio al source l'esito POSITIVO dell'invio del messaggio
        sendNumber(sd_source, &dest->listener_port, 16);                //invio la porta del dest al source
        printf("> %s ha inviato un firstMsg a %s\n", source->username, dest->username);
    }
    else{                                                               //dest non è online
        pushNewPendingMsg(source->username, dest->username, buffer);
        buf8 = 0; sendNumber(sd_source, &buf8, 8);                      //NON TOGLIERE: invio al source l'esito NEGATIVO dell'invio del messaggio
        printf("> memorizzato un msg da %s per %s\n", source->username, username_dest);
    }
}
void chat_anotherPendingMsg_s(int sd_source){                           //funzione che viene chiamata quando un device ha già un messaggio pendente verso un destinatario e ne vuole mandare un altro
    //so già che dest è offline, devo quindi solo memorizzare i messaggi
    char username_dest[20];
    char username_source[20];

    recvInfo(sd_source);
    strncpy(username_dest, buffer, strlen(buffer)+1);
    recvInfo(sd_source);
    strncpy(username_source, buffer, strlen(buffer)+1);
    recvText(sd_source);                                                //ricevo il testo del messaggio

    pushNewPendingMsg(username_source, username_dest, buffer);
    printf("> memorizzato un msg da %s per %s\n", username_source, username_dest);
}
void hanging_s(int sd_source){                                          //funzione lato server del comando hanging
    char request_username[INFO_SIZE];
    char buffer_time[INFO_SIZE];
    struct HangingBox* p_hb;
    struct PendingMsg* p_pm;
    struct Mailbox* p_mailboxDest;
    uint16_t howManySource = 0;
    struct tm *dateConverter;
    
    hangingBoxes = NULL;
    recvInfo(sd_source);
    strncpy(request_username, buffer, strlen(buffer)+1);
    p_mailboxDest = findMailbox(request_username);

    //DOVE VENGONO MEMORIZZATI I MSG PENDENTI?
    //per ogini utente destinatario di almeno un msg pendete viene istanziata una Mailbox, dove verranno memorizzati
    //tutti i msg pendenti a suo carico
    //
    //COME AVVIENE L'HANGING?
    //dato l'utente A di cui si vuole fare l'hanging, si cercano tutti i msg pendenti di A nella sua Mailbox.
    //Per ognuo dei diversi mittenti di questi msg viene istanziata una struttura TEMPORANEA chiamata HangingBox
    //dove di ogni mittente viene salvato il numero di messaggi inviati ed il timestamp dell'ultimo di questi msg

    if(!p_mailboxDest){   //se l'utente non ha nessun messaggio sospeso
        buf16 = 0; sendNumber(sd_source, &buf16, 16);
        return;
    }

    for(p_pm=p_mailboxDest->listMsg; p_pm != NULL ; p_pm = p_pm->next){     //scorro tutta la mailbox del dest

        //vedo se ho già una box per questo mittente
        p_hb = findHangingBox(p_pm->username_source);
        if(!p_hb){
            //creo una nuova HangingBox
            p_hb = (struct HangingBox*) malloc(sizeof(struct HangingBox));
            strncpy(p_hb->username_source, p_pm->username_source, strlen(p_pm->username_source)+1);
            p_hb->msgCounter = 1;
            p_hb->lastTS = p_pm->ts;
            p_hb->next = NULL;
            //inserisco (in testa) questa nuova HangingBox alla lista
            p_hb->next = hangingBoxes;
            hangingBoxes = p_hb;
        }
        else{
            p_hb->msgCounter += 1;
            p_hb->lastTS = p_pm->ts;
        }
    }

    //conto quanti source diversi hanno inviato un msg
    for(p_hb=hangingBoxes; p_hb != NULL ; p_hb = p_hb->next)
        howManySource += 1;                                             //invio numero dei source che mi gli hanno inviato almeno un msg
    
    //invio risposta
    buf16 = howManySource;  sendNumber(sd_source, &buf16, 16);
    for(p_hb=hangingBoxes; p_hb != NULL ; p_hb = p_hb->next){
        sendInfo(sd_source, p_hb->username_source);
        buf16 = p_hb->msgCounter; sendNumber(sd_source, &buf16, 16);

        dateConverter = localtime(&p_hb->lastTS);
        strftime(buffer_time, 20, "%Y-%m-%d %H:%M:%S", dateConverter);
        sendInfo(sd_source, buffer_time);
    }

    //devo ora fare pulizia
    while(hangingBoxes != NULL){
        p_hb = hangingBoxes;
        hangingBoxes = hangingBoxes->next;
        free(p_hb);
    }
}
void show_s(int sd_source){                                             //funzione lato server del comando show
    char username_source[20];
    char username_dest[20];
    struct Mailbox* p_mailboxDest, * p_m, * victim_m;
    struct PendingMsg* p_pm, * victim_pm;
    struct UserInfo* p_source;
    uint16_t counter = 0;
    
    recvInfo(sd_source);
    strncpy(username_dest, buffer, strlen(buffer)+1);
    recvInfo(sd_source);
    strncpy(username_source, buffer, strlen(buffer)+1);

    p_mailboxDest = findMailbox(username_dest);

    if(!p_mailboxDest || !p_mailboxDest->listMsg){                      //se l'utente non ha nessun messaggio sospeso
        buf16 = 0; sendNumber(sd_source, &buf16, 16);
        return;
    }

    for(p_pm=p_mailboxDest->listMsg; p_pm != NULL ; p_pm = p_pm->next){ //conto i messaggi in sospeso inviati dall'utente "username_source"
        if(strcmp(p_pm->username_source, username_source)==0)
            counter += 1;
    }
    buf16 = counter; sendNumber(sd_source, &buf16, 16);

    for(p_pm=p_mailboxDest->listMsg; p_pm != NULL ; p_pm = p_pm->next){
        if(strcmp(p_pm->username_source, username_source)==0){
            strncpy(buffer, p_pm->text, strlen(p_pm->text)+1);
            sendText(sd_source);
        }
    }

    //DEVO DIRE AL SOURCE CHE IL DEST HA FATTO SHOW
    if(counter > 0){
        printf("> %s ha scaricato i messaggi pendenti inviati da %s\n", username_dest, username_source);
        p_source = findUserInfo(username_source);
        if(isOnline(p_source)){
            buf8 = 2; sendNumber(p_source->socket_s, &buf8, 8);         //dico a source che dest ha ricevuto i messaggi
            sendInfo(p_source->socket_s, username_dest);
        }
    }

    //faccio "free" sui messaggi inviati dal source indicato nella mailbox del dest
    p_pm = p_mailboxDest->listMsg;
    p_mailboxDest->listMsg = NULL;
    while(p_pm){
        if(strcmp(p_pm->username_source, username_source)==0){
            victim_pm = p_pm;
            p_pm = victim_pm->next;
            free(victim_pm->text);
            free(victim_pm);
        }
        else{
            if(!p_mailboxDest->listMsg)
                p_mailboxDest->listMsg = p_pm;                          //l'head punta al primo che sopravvive

            p_pm = p_pm->next;
        }
    }

    //se la mailbox del dest è vuota allora rilascio anche la mailbox
    if(!p_mailboxDest->listMsg){
        p_m = mailboxes;
        mailboxes = NULL;

        while(p_m){
            if(strcmp(p_m->username_dest, username_dest)==0){
                victim_m = p_m;
                p_m = victim_m->next;
                free(victim_m);
                break;                                                  //se lo trovo posso uscire dal ciclo, dato che l'elemento da eliminare è solo uno
            }
            else{
                if(!mailboxes)
                    mailboxes = p_m;
                p_m = p_m->next;
            }
        }
    }
}
void out_s(int sd_source){                                              //funzione lato server del comando out
    struct UserInfo* p_source = findUserInfo_bySocketChat(sd_source);
    FILE* fp;

    p_source->ts_logout = time(NULL);
    p_source->listener_port = 0;
    
    close(p_source->socket_s);                                          //chiudo la connessione con l'utente
    FD_CLR(p_source->socket_s, &master);
    
    p_source->socket_s = -1;

    fp = fopen("log.txt", "a");                                         //salvo sul log l'uscita da parte dell'utente
    if(fp==NULL){ perror("Errore in apertura del file"); exit(-1); }
    fprintf(fp,"\n%s %ld %ld", p_source->username, p_source->ts_login, p_source->ts_logout);
    fclose(fp);

    printf("> %s è andato offline\n", p_source->username);
}

///----------FUNZIONI AUSILIARIE (implementazione)---------
int newListenerSocket(){
    struct sockaddr_in server_addr;
    int sd, yes=1;

    sd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));        //libera subito la porta al termine del processo
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(srv_port);
    inet_pton(AF_INET, "localhost", &server_addr.sin_addr);    
    ret = bind(sd, (struct sockaddr*) &server_addr, sizeof(server_addr));
    if(ret<0){ perror("ERRORE BIND"); exit(-1); }
    ret = listen(sd, 10);

    return sd;
}
void newCommunicationSocket(){
    int addrlen, newfd;
    struct sockaddr_in client_addr;

    addrlen = sizeof(client_addr);
    newfd = accept(listener, (struct sockaddr *) &client_addr, (socklen_t*)&addrlen);
    FD_SET(newfd, &master);
    if(newfd > fdmax)
        fdmax = newfd;                                                  //Aggiorno fdmax
}

void readDatabase(){                                                    //funzione che legge i contatti salvati nel database e li carica in una struttura in memoria
    FILE* fp;
    char* res;
    struct UserInfo* tmp_punt;

    /* apre il file */
    fp=fopen("database.txt", "r");
    if(fp==NULL){ perror("Errore in apertura del file"); exit(-1); }

    /* legge e stampa ogni riga */
    while(1) {
        res=fgets(buffer, BUFFER_SIZE, fp);
        if(res==NULL)
            break;

        tmp_punt = (struct UserInfo*) malloc(sizeof(struct UserInfo));  //qui la newUserInfo non la posso usare
        //interpretazione riga
        sscanf(buffer, "%s %s", tmp_punt->username, tmp_punt->password);
        tmp_punt->listener_port = 0;
        tmp_punt->socket_s = -1;
        tmp_punt->ts_login = 1;
        tmp_punt->ts_logout = 1;
        tmp_punt->next = NULL;
        pushUserInfo(&database, tmp_punt);
    }
    fclose(fp);
}
void writeNewRowDatabase(struct UserInfo* new_data){                    //funzione che scrive un nuovo record nel database
    FILE* fp;

    fp=fopen("database.txt", "a");
    if(fp==NULL){ perror("Errore in apertura del file"); exit(-1); }
    fprintf(fp,"\n%s %s", new_data->username, new_data->password);
    fclose(fp);
}
void askIfOnline_s(int sd_source){                                      //funzione che resituisce lo stato di un utente
    char searched_username[INFO_SIZE];
    struct UserInfo* p_user;

    recvInfo(sd_source);
    strncpy(searched_username, buffer, strlen(buffer)+1);

    p_user=findUserInfo(searched_username);
    if(!p_user)     //non esiste nel database
        buf8=2;
    else if(isOnline(p_user))
        buf8=1;
    else            //se offline
        buf8=0;

    sendNumber(sd_source, &buf8, 8);
}
void askPort_s(int sd_source){
    struct UserInfo* p_user;

    recvInfo(sd_source);
    p_user = findUserInfo(buffer);
    sendNumber(sd_source, &p_user->listener_port, 16);
}
bool isOnline(struct UserInfo* p){                                      //funzione che dato un utente, restituisce true se questo è offline e false altrimenti
    if(p->ts_logout==0)
        return true;
    else
        return false;
}

///----------FUNZIONI DI UTILITÀ (implementazione)---------
void sendText(int sd_dest){                                             //funzione che invia una stringa di testo di lunghezza < BUFFER_SIZE
    uint16_t lmsg;

    lmsg = strlen(buffer)+1;
    sendNumber(sd_dest, &lmsg, 16);

    ret = send(sd_dest, (void*) buffer, lmsg, 0);
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

struct UserInfo* newUserInfo(char* newUsername, char* newPassword){     //funzione che permette di memorizzare le info di un contatto
    struct UserInfo* tmp_punt = (struct UserInfo*) malloc(sizeof(struct UserInfo));

    strncpy (tmp_punt->username, newUsername, INFO_SIZE);
    strncpy (tmp_punt->password, newPassword, INFO_SIZE);
    tmp_punt->listener_port = 0;
    tmp_punt->socket_s = -1;
    tmp_punt->ts_login = 1;
    tmp_punt->ts_logout = 1;
    tmp_punt->next = NULL;

    return tmp_punt;
}
void pushUserInfo(struct UserInfo** head, struct UserInfo* new_data){   //funzione che inserisce una struct UserInfo in lista (inserimento in testa)
    new_data->next = *head;
    *head = new_data;
}
void pushNewPendingMsg(char* newUsername_source, char* newUsername_dest, char* newText){    //funzione che inserisce una struct PendingMsg in lista (inserimento in coda)
    struct PendingMsg* p_newMsg, *p;
    struct Mailbox* p_mailbox = findMailbox(newUsername_dest);

    if(!p_mailbox){
        //ne creo uno
        p_mailbox = (struct Mailbox*) malloc(sizeof(struct Mailbox));
        strncpy(p_mailbox->username_dest, newUsername_dest, strlen(newUsername_dest)+1);
        p_mailbox->listMsg = NULL;
        //e lo metto in mailboxes
        p_mailbox->next = mailboxes;
        mailboxes = p_mailbox;
    }

    //creo un nuovo PendingMsg
    p_newMsg = (struct PendingMsg*) malloc(sizeof(struct PendingMsg));
    uint16_t len = (strlen(newText))+1;
    p_newMsg->text = (char*) malloc(len);

    strncpy(p_newMsg->username_source, newUsername_source, strlen(newUsername_source)+1);
    p_newMsg->ts = time(NULL);
    strncpy(p_newMsg->text, newText, len);
    p_newMsg->next = NULL;

    //ora inserisco questo nuovo PendingMsg in fondo alla Mailbox
    if(p_mailbox->listMsg == NULL)
        p_mailbox->listMsg = p_newMsg;
    else{
        for(p=p_mailbox->listMsg; p->next != NULL; p=p->next)
            ;
        p->next = p_newMsg;
    }
}

struct UserInfo* findUserInfo(char* searched_username){                 //funzione che cerca un contatto nella rubrica a partire dal suo username
    struct UserInfo* p;
    if(!database) return NULL;

    for(p=database; (p!=NULL) && (strcmp(p->username, searched_username)!=0); p=p->next)
        ;
    
    return p;
}
struct UserInfo* findUserInfo_bySocketChat(int searched_sd){            //funzione che cerca un contatto nella rubrica a partire dal suo sd
    struct UserInfo* p;
    if(!database) return NULL;

    for(p=database; (p!=NULL) && (p->socket_s != searched_sd); p=p->next)
        ;
    
    return p;
}
struct Mailbox* findMailbox(char* searched_username){                   //funzione che cerca in memoria la mailbox di un utente
    struct Mailbox* p;
    if(!mailboxes) return NULL;

    for(p=mailboxes; (p!=NULL) && (strcmp(p->username_dest, searched_username)!=0); p=p->next)
        ;
    
    return p;
}
struct HangingBox* findHangingBox(char* searched_username){             //funzione che cerca in memoria la hangingBox di un utente
    struct HangingBox* p;
    if(!hangingBoxes) return NULL;

    for(p=hangingBoxes; (p!=NULL) && (strcmp(p->username_source, searched_username)!=0); p=p->next)
        ;
    
    return p;
}





