#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/msg.h>
#include <string.h>

#define CLEAR_BUFFER() do { int ch; while ( (ch = getchar()) != EOF && ch != '\n' ) {} } while (0)

#define MSGQ_KEY 1234 

#define REQ 1
#define RSP 2
#define REQ_SIZE (sizeof(struct request)-sizeof(long))
#define RSP_SIZE (sizeof(struct response)-sizeof(long))

struct request {
    long type; // REQ
    int mqid; // message queue id to use for reply
    char msg[2];
};

struct response {
    long type; // RSP
    int key; // shared memory
};

struct shared_memory {
    int letters;
    int attempts;
    int hits;
    int winlose;
    int pst[13];
    char lett;
    char first_l;
    char last_l;
    char Lword[13];
    pid_t server_pid;
    pid_t client_pid;
};

static volatile sig_atomic_t gotsig = -1; // universal signal variable
static void handler(int sig) { gotsig = sig; }

void the_Hangman_Game(struct shared_memory *mem);

int main(){

    struct sigaction act = { {0} };
    act.sa_handler = handler;
    sigaction(SIGUSR1,&act,NULL);

    pid_t pclient = getpid();

    int mqid1;
    struct request req; struct response rsp;

    mqid1 = msgget(MSGQ_KEY,IPC_PRIVATE | S_IRWXU);
    if( mqid1 < 0 ){
        printf("Msg Queue Failed\n");
        exit(-1);
    }
    req.type = REQ; req.mqid = mqid1;
    
    strcpy(req.msg,"hi");
    
    msgsnd(mqid1,&req,REQ_SIZE,0);

    msgrcv(mqid1,&rsp,RSP_SIZE,RSP,0);

    int shmid;

    shmid = shmget(rsp.key,0,0);
    struct shared_memory *memory = (struct shared_memory *) shmat(shmid,NULL,0);

    memory->client_pid = pclient;

    printf("Client Connected\n");
    printf("Waiting for the server to prepare the settings\n");
    kill(memory->server_pid,SIGUSR1);
    pause();

    the_Hangman_Game(memory);

    printf("Exiting Client...\n");
    shmdt(memory);
    msgctl(mqid1,IPC_RMID,NULL);
    
    return 0;
}

void the_Hangman_Game(struct shared_memory *mem){
    printf("Welcome!!!\n");
    int size = mem->letters;
    int atms = mem->attempts;
    char word[size];
    char ch;
    int i;
    
    for( i = 0; i < size; i++ ){
        word[i] = '_';
    }
    word[0] = mem->first_l;
    word[size-1] = mem->last_l;

    while( 1 ){
        for ( i = 0; i < size; i++){
            printf("%c",word[i]);
        }
        printf("\n");
        printf("Enter a letter: ");
        scanf("%c",&ch);
        CLEAR_BUFFER();
        mem->lett = ch;

        kill(mem->server_pid,SIGUSR1);
        pause();

        printf("You find %d letters\n",mem->hits);
        printf("You have %d attempts\n",mem->attempts);
        
        if( mem->pst[0] != -1 ){
            i = 0;
            while( mem->pst[i] != -1 ){
                word[mem->pst[i]-1] = ch;
                i++;
            }
        }

        if( mem->winlose == 0 ){
            printf("\nThe word was : ");
            printf("%s\n",mem->Lword);
            
            printf("You lost\n");
            break;
        }

        if( mem->winlose == 1 ){
            printf("\nThe word was : ");
            for ( i = 0; i < size; i++){
                printf("%c",word[i]);
            }
            printf("\n");
            printf("You Won\n");
            break;
        }

    }

}