#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>

#define MAX_SIZE_BUFFER 1024
#define MAX_COMMANDS 10
#define MAX_BUFFER 1024

/*---------- Variaveis Globais ----------*/
char *names[5];
char *filters[5];
int max_instance[5];
int open_instances[5];
char *tasks[2048];
int nextTask;





char * line ;
int line_index = 0;
int line_end = 0;


int searchFilterIndex(char* filterName){
    int i = 0;
    while (strcmp(filterName, filters[i]) != 0){
        i++;
    }
    return i;
}


ssize_t readln(int fildes, void *buf, size_t nbyte){
    ssize_t res = 0;
    int i = 0;

    while (i < nbyte && (res = read (fildes, &buf[i], 1)) > 0){
        if (((char *) buf)[i] == '\n') return i+1;
        i += res;
    }
    return i;
}

char** cmd_parser (char* cmd_list, int *n_cmds){
    int i = 0;
    char** parsed_cmds = malloc(sizeof(char*)*MAX_COMMANDS);

    char* token;

    token = strtok(cmd_list, " ");
    while (i < MAX_COMMANDS && token != NULL){
        parsed_cmds[i] = strdup(token);
        token = strtok(NULL, " ");
        i++;
    }

    (*n_cmds) = i;

    return parsed_cmds;
}

int n_lines_config(){
    int result;
    int fildes[2];
    pipe(fildes);
    if(!fork()){
        close(1);
        close(fildes[0]);
        dup2(fildes[1],STDOUT_FILENO);
        close(fildes[1]);
        execlp("awk","awk","END {print NR}", "../etc/aurrasd.conf", NULL);
    }
    else{
        close(fildes[1]);
        char* buffer = malloc(sizeof(10));
        read(fildes[0], buffer, 10);
        close(fildes[0]);
        result = atoi(buffer);
    }
    return result;
}

void read_config(char** names, char** filters, int* max_instance){
    char* line = malloc(sizeof(char)*100);
    char** parsed_line;
    int counter = 0;
    int ignore;
    int fd = open("../etc/aurrasd.conf", O_RDONLY, 0666);
    ssize_t size;
    while ((size = readln(fd, line, 100)) > 0){
        line[size] = '\0';
        parsed_line = cmd_parser(line, &ignore);
        names[counter] = strdup(parsed_line[0]);
        filters[counter] = strdup(parsed_line[1]);
        max_instance[counter] = atoi(parsed_line[2]);
        counter++;
    }
}

void executeTask(int n_filters, char **f_a_executar, char input_file[]){
    int fp [n_filters + 1][2];
    int pid;
    int current_pipe = 0;
    char *buffer = malloc(sizeof(char) * 1024);
    char *aux = malloc(sizeof(char) * 25);
    buffer = strdup("\nmensagem para teste\n");
    int filter = 0;
    int nb_read;
    int ft = open("teste.txt",O_RDONLY,0666);
    int ftf = open("final.txt", O_CREAT | O_RDWR, 0666);
    int status;
    pipe(fp[current_pipe]);
    int indexes[n_filters];
    for (int i = 0; i < n_filters; i++){
        indexes[i] = searchFilterIndex(f_a_executar[i]);
    }
    switch (pid = fork()) {
        case 0://processo filho de controlo
            close(fp[current_pipe][1]);
            while(current_pipe <= n_filters){
                pipe(fp[current_pipe+1]);
                switch (fork()){
                    case 0:
                        while((nb_read = read(fp[current_pipe][0],buffer,1024)) > 0){
                            sprintf(aux,"filho%d:lerficheiro\n",filter);
                            write(STDOUT_FILENO,aux,25);
                            write(fp[current_pipe+1][1],buffer,nb_read);
                        }
                        sprintf(aux,"fim filho%d:\n",filter);
                        write(STDOUT_FILENO,aux,25);
                        close(fp[current_pipe][0]);
                        close(fp[current_pipe+1][1]);
                        exit(0);
                        break;
                    default:
                        close(fp[current_pipe][0]);   //fechar o descritor de leitura do pipe anterior para nao ser herdado por mais nenhum processo
                        close(fp[current_pipe+1][1]); //fechar o descritor de escrita do proximo pipe para nao ser herdado por mais nenhum processo
                        wait(&status);
                        break;
                }
                current_pipe++;
                filter++;
            }
            sprintf(aux,"fim dos filhos\n");
            write(STDOUT_FILENO,aux,25);
            while((nb_read = read(fp[current_pipe][0],buffer,1024)) > 0){
                sprintf(aux,"escrever final\n");
                write(STDOUT_FILENO,aux,25);
                write(ftf,buffer,nb_read);
            }
            sprintf(aux,"final\n");
            write(STDOUT_FILENO,aux,25);
            exit(0);
            break;
        default: //processo pai:
            while ((nb_read = read(ft, buffer, 1024)) > 0) {
                write(fp[current_pipe][1],buffer,nb_read);
            }
            printf("pai: ler ficheiro \n");
            close(fp[current_pipe][1]);
            wait(&status);
            for (int i = 0; i < n_filters; i++){
                open_instances[indexes[i]]--;
            }
            break;
    }
}

int main(int argc, char* argv[]){

    //int n_filters = n_lines_config();
    //char** names = malloc(sizeof(char*)*n_filters);
    //char** filters = malloc(sizeof(char*)*n_filters);
    //int* max_instance = malloc(sizeof(int)*n_filters);
    for (int i = 0 ; i < 5 ; i++){
        open_instances[i] = 0;
    }
    read_config(names, filters, max_instance);
    char s[4] = "ola";
    char ** f_a_executar = malloc(sizeof(char*) * 3);
    f_a_executar[0] = strdup("aurrasd-gain-double");
    f_a_executar[1] = strdup("aurrasd-gain-half");
    f_a_executar[2] = strdup("aurrasd-tempo-half");
    
    executeTask(3,f_a_executar,s);
    for(int i = 0; i < 5 ; i++){
        printf("%d, ", open_instances[i]);
    }
    /*
    if (mkfifo("fifo", 0666) == -1){
        perror ("Fifo not created");
    }

    int bytes_read;
    char* buffer = malloc(MAX_SIZE_BUFFER * sizeof(char));
    int fdr, fdw;

    if ((fdr = open("fifo", O_RDONLY)) == -1){
        perror("Open FIFO for reading");
    }
    if ((fdw = open("fifo", O_WRONLY)) == -1){
        perror("Open FIFO for writing");
    }


    char** commands;
    int n_cmds;

    while((bytes_read = read(fdr, buffer, MAX_SIZE_BUFFER)) > 0){
        commands = cmd_parser(buffer, &n_cmds);
        for (int i = 0; i < n_cmds; i++){
            printf ("%s\n", commands[i]);
        }
    }
    close (fdw);
    close (fdr);
    unlink("fifo");

    */


    return 0;
}
