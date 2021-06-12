#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>


#define TRUE 1
#define FALSE 0


//SIG1 -> Processing 
//SIG2 -> Pending

void sig1_handler(int sig2){
    write(STDOUT_FILENO, "#Processing\n", 13);
}


void sig2_handler(int sig1){
    write(STDOUT_FILENO, "#Pending\n", 10);
}


int main(int argc, char* argv[]){
    signal(SIGUSR1,sig1_handler);
    signal(SIGUSR2,sig2_handler);
    int client_server = open("./tmp/client_server", O_WRONLY, 0666);
    int server_client = open("./tmp/server_client", O_RDONLY, 0666);
    int status_pipe = open("./tmp/status",O_WRONLY,0666);
    close(status_pipe);
    pid_t pid = getpid();
    char* task = malloc(sizeof(char) * 1024);
    //transform input output [... filters ...]
    int nb_read;
    char* ignore = malloc(sizeof(char) * 20);
    sprintf (ignore, "ignore_this_message");
    char *status = malloc(sizeof(char)*1024);
    int read_flag = FALSE;
    if (argc > 1){
        if (strcmp(argv[1],"status") == 0){
            status_pipe = open("./tmp/status",O_WRONLY,0666);
            write(status_pipe, argv[1],strlen(argv[1]));
            write(client_server, ignore, 19);
            while(!read_flag && (nb_read = read(server_client, status, 1024)) > 0){
                write(STDOUT_FILENO, status, nb_read);
                read_flag = TRUE;
            }
            close(status_pipe);
        }
        else if ((strcmp(argv[1], "transform") == 0) && argc >= 5){
            task[0] = '\0';
            char* pid_string = malloc(sizeof(char)*10);
            sprintf (pid_string, "%d ", pid);
            strcat(task, pid_string);
            for (int i = 1; i < argc; i++){
                strcat(task, argv[i]);
                strcat(task, " ");
            }
            write(client_server, task, strlen(task));
            printf("%s\n", task);
            while(1);
        }
        else {
            char info[95] = "./aurras status \n./aurras transform input-filename output-filename filter-id-1 filter-id-2 ...\n";
            printf ("Aurras usage: \n%s", info);
        }
    }
    else{
        char info[95] = "./aurras status \n./aurras transform input-filename output-filename filter-id-1 filter-id-2 ...\n";
        printf ("Aurras usage: \n%s", info);
    }
    
    printf ("cheguei antes do while\n");

    //while(1);

    return 0;
}