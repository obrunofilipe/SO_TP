#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>

/*---------------- MACROS ---------------*/
#define MAX_SIZE_BUFFER 1024
#define MAX_COMMANDS 10
#define MAX_BUFFER 1024
#define N_FILTERS 5
#define TRUE 1
#define FALSE 0


/*---------- Variaveis Globais ----------*/

int n_filters;
char *names[5];
char *filters[5];
int max_instance[5];
int open_instances[5];
char *tasks[1024];
char* tasks_final[1024];
pid_t pids[1024];
char *waiting_tasks[512];
int current_waiting = 0;
int next_waiting = 0;
int nextTask;
int ongoing_tasks = 0;
int current_task = 0;


int checkFilter(char** f_to_exec, int number_f){
    int index;
    int res = FALSE;
    int array_filters[n_filters];
    for (int i = 0; i < number_f; i++) array_filters[i] = 0;
    for (int i = 0; i < number_f; i++){
        index = searchFilterIndex(f_to_exec[i]);
        array_filters[index]++;
    }
    for (int i = 0; i < n_filters; i++){
        if ((array_filters[i] + open_instances[i]) >= max_instance[i]) return FALSE;
    }
    return TRUE;
}

char** cmd_parser (char* cmd_list, int *n_cmds);


char *line ;
int line_index = 0;
int line_end = 0;

int pid_index(pid_t pid){
    int i = 0;
    while (pids[i] != pid){
        i++;
    }
    return i;
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

void printStatus(int n_filters){
    int i = 0;
    char* line_to_print = malloc(sizeof(char*) * 128);
    while (i < ongoing_tasks){
        if (strcmp(tasks_final[i], "acabou") != 0){
            write(STDOUT_FILENO, tasks_final[i], 512);
            i++;
        }
    }
    for (int i = 0; i < n_filters; i++){
        sprintf(line_to_print, "Filter %s: %d/%d (running/max)\n", names[i], open_instances[i], max_instance[i]);
        write (STDOUT_FILENO, line_to_print, 128);
    }
}

// transform /home/user/samples/sample-1.m4a output-1.m4a baixo rapido 
// filtros a partir do 3 
void sigchld_handler(int sig){
    pid_t pid;
    int status, n_cmds;
    int index_task;
    char** parsed_task;
    int i = 0;
    char* filter;
    pid = waitpid(-1, &status, WNOHANG);
    index_task = pid_index(pid);
    parsed_task = cmd_parser(tasks[index_task], &n_cmds);
    for (int i = 5 ; i < n_cmds ; i++){
        filter = filterWithName(parsed_task[i]);
        open_instances[searchFilterIndex(filter)]--;
    }
    tasks_final[index_task] = "acabou";
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
    switch (pid = fork()) {
        case 0: //processo filho de controlo
            close(fp[current_pipe][1]);
            while(current_pipe <= n_filters){
                pipe(fp[current_pipe+1]);
                switch (fork()){
                    case 0:
                        while((nb_read = read(fp[current_pipe][0],buffer,1024)) > 0){
                            write(fp[current_pipe+1][1],buffer,nb_read);
                        }
                        sleep(2);
                        close(fp[current_pipe][0]);
                        close(fp[current_pipe+1][1]);
                        exit(0);
                        break;
                    default:
                        close(fp[current_pipe][0]);   //fechar o descritor de leitura do pipe anterior para nao ser herdado por mais nenhum processo
                        close(fp[current_pipe+1][1]); //fechar o descritor de escrita do proximo pipe para nao ser herdado por mais nenhum processo
                        break;
                }
                current_pipe++;
                filter++;
            }
            while((nb_read = read(fp[current_pipe][0],buffer,1024)) > 0){
                write(ftf,buffer,nb_read);
            }
            //
            exit(0);
            break;
        default: //processo pai:
            while ((nb_read = read(ft, buffer, 1024)) > 0) {
                write(fp[current_pipe][1],buffer,nb_read);
            }
            close(fp[current_pipe][1]);
            pause();
            break;
    }
    ongoing_tasks--;
}

int main(int argc, char* argv[]){
    int status;
    
    pid_t pid;
    for (int i = 0 ; i < 5 ; i++){
        open_instances[i] = 0;
    }
    read_config(names, filters, max_instance);
    char s[4] = "ola";

    char** f_a_executar;
    

    char **parsed_commands;
    char task_array_test[4][512] = {"transform /home/user/samples/sample-1.m4a output-1.m4a baixo rapido ", "transform /home/user/samples/sample-1.m4a output-1.m4a baixo rapido eco lento alto", "transform /home/user/samples/sample-2.m4a output-3.m4a rapido rapido baixo eco", "transform /home/user/samples/sample-2.m4a output-3.m4a lento lento baixo eco"};
    int n_commands;
    char* task_to_be_parsed = malloc(sizeof(char)*128);
    int filters_exec;
    int end_task_values[5];

    int task_counter = 1;
    signal(SIGCHLD, sigchld_handler);
    while (task_counter < 5){
        if (current_waiting == next_waiting){
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
            tasks_final[task_counter-1] = strdup(tasks[task_counter-1]);

            if (checkFilters(f_a_executar, filters_exec)){
                if((pid = fork()) == 0){
                    write (STDOUT_FILENO, "Started task\n", 14);
                    executeTask(filters_exec, f_a_executar, s, task_counter);
                    exit(0);
                }
                else{
                    char* filter;
                    for(int i  = 0; i < filters_exec; i++){
                        filter = filterWithName(f_a_executar[i]);
                        open_instances[searchFilterIndex(filter)]++;
                        free(filter);
                    }
                    pids[current_task] = pid;
                    current_task++;
                }
            }
            else{
                waiting_tasks[next_waiting] = strdup(tasks[task_counter-1]);
                next_waiting++;
            }
            
        }
        else{
            sprintf (task_to_be_parsed, "%s", waiting_tasks[task_counter-1]);
            parsed_commands = cmd_parser(task_to_be_parsed, &n_commands);
            filters_exec = n_commands-3;
            
            f_a_executar = malloc(sizeof(char*) * (filters_exec));

            for (int i = 3; i < n_commands; i++){
                f_a_executar[i-3] = malloc(sizeof(char)*10);
                sprintf (f_a_executar[i-3], "%s", filterWithName(parsed_commands[i]));
            }
            

            tasks[task_counter-1] = malloc(sizeof(char) * 512);
            sprintf (tasks[task_counter-1], "task #%d: %s", task_counter, task_array_test[task_counter-1]);
            tasks_final[task_counter-1] = strdup(tasks[task_counter-1]);

            if (checkFilters(f_a_executar, filters_exec)){
                if((pid = fork()) == 0){
                    write (STDOUT_FILENO, "Started task\n", 14);
                    executeTask(filters_exec, f_a_executar, s, task_counter);
                    exit(0);
                }
                else{
                    char* filter;
                    for(int i  = 0; i < filters_exec; i++){
                        filter = filterWithName(f_a_executar[i]);
                        open_instances[searchFilterIndex(filter)]++;
                        free(filter);
                    }
                    pids[current_task] = pid;
                    current_task++;
                }
            }
            else{
                waiting_tasks[next_waiting] = strdup(tasks[task_counter-1]);
                next_waiting++;
            }

        }
       

        printf ("\n");
        task_counter++;
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

   //while(wait(&status));
    //while(1);
    return 0;
}


/*
if((pid = fork()) == 0){
                write (STDOUT_FILENO, "Started task\n", 14);
                executeTask(filters_exec, f_a_executar, s, task_counter);
                exit(0);
            }
            else{
                char* filter;
                for(int i  = 0; i < filters_exec; i++){
                    filter = filterWithName(f_a_executar[i]);
                    open_instances[searchFilterIndex(filter)]++;
                    free(filter);
                }
                pids[current_task] = pid;
                current_task++;
            }
*/