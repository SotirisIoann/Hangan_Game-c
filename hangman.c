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
#include <time.h>
#include <string.h>

#define CLEAR_BUFFER() do { int ch; while ( (ch = getchar()) != EOF && ch != '\n' ) {} } while (0)

#define BUFSIZE 110

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

char **get_Words(char *txt,int *size);
void the_Hangman(char **words,struct shared_memory *mem,int size);
int find_word(int size);

int main(int argc, char *argv[]){

    if( argc < 2 ){
        printf("Pass the dictionary to start the Server\n./executable dictionary.txt\n");
        exit(-1);
    }

    int i;
    int size;
    char *txt = argv[1];
    char **words;

    struct sigaction act = { {0} };
    act.sa_handler = handler;
    sigaction(SIGUSR1,&act,NULL);

    pid_t pserver = getpid();

    words = get_Words(txt,&size);

    int mqid;
    struct request req; struct response rsp={0};

    mqid = msgget(MSGQ_KEY, IPC_CREAT | S_IRWXU);
    if( mqid < 0 ){
        printf("Msg Queue Failed\n");
        exit(-1);
    }

    int key = ftok(".",'a');
    
    int shmid =  shmget(key, 0, S_IRWXU);
    size_t msize = sizeof(struct shared_memory);
    shmctl(shmid, IPC_RMID, NULL);
    shmid = shmget(key, msize, IPC_CREAT | IPC_EXCL | S_IRWXU);
    struct shared_memory *memory = (struct shared_memory *) shmat(shmid,NULL,0);

    memory->server_pid = pserver;
    
    printf("Waiting for Client to connect\n");
    msgrcv(mqid,&req,REQ_SIZE,0,0);

    printf("Conneted...\nServer receive %s from client\n",req.msg);
    rsp.type = RSP;
    rsp.key = key;

    msgsnd(req.mqid, &rsp, RSP_SIZE, 0);
    
    pause();
    the_Hangman(words,memory,size);

    printf("Exiting Server...\n");
    shmdt(memory);
    msgctl(mqid,IPC_RMID,NULL);
    
    return 0;
}

char **get_Words(char *txt,int *size){

    int fd = open(txt, O_RDWR, S_IRWXU);
    if( fd < 0 ){
        printf("%s: There is no such a file\n",txt);
        exit(-1);
    }

    char **words = (char**) malloc(BUFSIZE * sizeof(char*));
    int j = 0;
    for( j = 0; j < BUFSIZE; j++ ){
        words[j] = (char*) malloc(12 * sizeof(char));
    }

    char c;
    int i = 0;
    j = 0;
    while( read(fd, &c, sizeof(char)) > 0 ){
        if( c == '\n' ){
            words[i][j] = '\0';
            j = 0;
            i++;
        }
        words[i][j++] = c;
    }

    *size = i;
    return words;
}

void the_Hangman(char **words, struct shared_memory *mem,int size){
    int wd;
    char ch;

    wd = find_word(size);
    
    int wordsize = strlen(words[wd]);

    mem->hits = 0;
    mem->winlose = -1;
    mem->letters = wordsize - 1;
    mem->first_l = words[wd][1];
    mem->last_l = words[wd][wordsize-1];
    mem->attempts = 6;
    
    kill(mem->client_pid,SIGUSR1);
    int j = 0;
    int i = 0;
    int tmp = 0;

    while(1){
        
        pause();
        
        ch = mem->lett;

        for( i = 0; i < 15; i++ ){
            mem->pst[i] = -1;
        }

        j = 0;
        i = 0;
        tmp = 0;

        for( j = 2; j < wordsize - 1; j++ ){
            if( words[wd][j] == ch ){
                mem->pst[i] = j;
                i++;
                tmp++;
            }
        }
        if( tmp == 0 ){
            mem->attempts = mem->attempts - 1;
        }else{
            mem->hits = mem->hits + tmp;
        }
        
        if( mem->attempts == 0 ){
            mem->winlose = 0;
            strcpy(mem->Lword,words[wd]);
            kill(mem->client_pid,SIGUSR1);
            break;
        }

        if( (mem->hits+2) == wordsize - 1 ){
            mem->winlose = 1;
            kill(mem->client_pid,SIGUSR1);
            break;
        }

        kill(mem->client_pid,SIGUSR1);

    }
    
}

int find_word(int size){
    time_t t;

    srand((unsigned) time(&t));

    return (rand() % (size + 1));
}