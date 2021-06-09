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
int n_filters;
char *names[5];
char *filters[5];
int max_instance[5];
int open_instances[5];
char *tasks[512];
char *waiting_tasks[512];
int nextTask;
int ongoing_tasks = 0;





char *line ;
int line_index = 0;
int line_end = 0;

void printStatus(int n_filters){
    int i = 0;
    char* line_to_print = malloc(sizeof(char*) * 128);
    while (i < ongoing_tasks){
        if (strcmp(tasks[i], "acabou") != 0){
            write(STDOUT_FILENO, tasks[i], 512);
            i++;
        }
    }
    for (int i = 0; i < n_filters; i++){
        sprintf(line_to_print, "Filter %s: %d/%d (running/max)\n", names[i], open_instances[i], max_instance[i]);
        write (STDOUT_FILENO, line_to_print, 128);
    }
}


int searchFilterIndex(char* filterName){
    int i = 0;
    while (strcmp(filterName, filters[i]) != 0){
        i++;
    }
    return i;
}

char* filterWithName (char* name){
    char* filter = malloc(sizeof(char)*30);
    int i = 0;
    while (strcmp(names[i], name) != 0){
        i++;
    }
    return strdup(filters[i]);
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
    char** parsed_cmds = malloc(sizeof(char*) * MAX_COMMANDS);

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
    n_filters = counter;
}

void executeTask(int n_filters, char **f_a_executar, char input_file[], int task){
    ongoing_tasks++;
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
    for (int i = 0; i < n_filters; i++){
        open_instances[indexes[i]]++;
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
                        sleep(2);
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
            //decrementar
            for (int i = 0; i < n_filters; i++){
                open_instances[indexes[i]]--;
            }
            tasks[task-1] = "acabou";
            printf("pai: ler ficheiro \n");
            close(fp[current_pipe][1]);
            break;
    }
    ongoing_tasks--;
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

    char** f_a_executar;
    

    char **parsed_commands;
    char task_array_test[4][512] = {"transform /home/user/samples/sample-1.m4a output-1.m4a baixo rapido ", "transform /home/user/samples/sample-1.m4a output-1.m4a baixo rapido eco lento alto", "transform /home/user/samples/sample-2.m4a output-3.m4a rapido rapido baixo eco", "transform /home/user/samples/sample-2.m4a output-3.m4a lento lento baixo eco"};
    int task_counter = 1;
    int n_commands;
    char* task_to_be_parsed = malloc(sizeof(char)*128);
    int filters_exec;
    while (task_counter < 5){
        //deverá estar dentro de um while para tratar cada pedido individualmente
        sprintf (task_to_be_parsed, "%s", task_array_test[task_counter-1]);
        parsed_commands = cmd_parser(task_to_be_parsed, &n_commands);
        filters_exec = n_commands-3;
        
        f_a_executar = malloc(sizeof(char*) * (filters_exec));

        for (int i = 3; i < n_commands; i++){
            f_a_executar[i-3] = malloc(sizeof(char)*10);
            sprintf (f_a_executar[i-3], "%s", filterWithName(parsed_commands[i]));
        }


        tasks[task_counter-1] = malloc(sizeof(char) * 512);
        sprintf (tasks[task_counter-1], "task #%d: %s", task_counter, task_array_test[task_counter-1]);
        

        executeTask(filters_exec, f_a_executar, s, task_counter);
        for(int i = 0; i < 5 ; i++){
            printf("%d, ", open_instances[i]);
        }
        printf ("\nDepois do execute_task");
        for (int i = 0; i < task_counter; i++){
            printf ("\n%s", tasks[i]);
        }


        printf ("\n");
        task_counter++;
    }
    sleep(5);

    printf ("\n\n\n STATUS\n");
    printStatus(n_filters);
    printf ("\n\n\n");

    printf ("Array de tasks: \n");
    for (int i = 0; i < 4; i++){
        printf ("%s\n", tasks[i]);
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
