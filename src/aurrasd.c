#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

/*---------------- MACROS ---------------*/

#define MAX_SIZE_BUFFER 1024
#define MAX_COMMANDS 10
#define MAX_BUFFER 1024
#define TRUE 1
#define FALSE 0
#define STD_SIZE 512
#define NAME_FILTER_SIZE 10


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
int number_of_tasks = 0;
int flag_block = TRUE;
int server_client;


int searchFilterIndex(char* filterName);

int checkFilters(char** f_to_exec, int number_f){
    int index;
    int array_filters[n_filters];
    for (int i = 0; i < n_filters; i++) array_filters[i] = 0;
    for (int i = 0; i < number_f; i++){
        index = searchFilterIndex(f_to_exec[i]);
        array_filters[index]++;
    }
    for (int i = 0; i < n_filters; i++){
        if ((array_filters[i] + open_instances[i]) > max_instance[i]) return FALSE;
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

int searchTaskIndex (char* task_to_compare){
    int i = 0;
    while (i < number_of_tasks && strcmp(task_to_compare, tasks[i]) != 0){
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
    int i = 0;
    while (strcmp(names[i], name) != 0){
        i++;
    }
    return strdup(filters[i]);
}

char* printStatus(int n_filters){
    int i = 0;
    char* line_to_print = malloc(sizeof(char*) * 1024);
    line_to_print[0] = '\0';
    char* final_print = malloc(sizeof(char*) * 1024);
    while (i < number_of_tasks){
        if (strcmp(tasks[i], "acabou") != 0){
            strcat(line_to_print, tasks[i]);
            strcat(line_to_print, "\n");
        }
        i++;
    }
    for (int i = 0; i < n_filters; i++){
        sprintf(final_print, "Filter %s: %d/%d (running/max)\n", names[i], open_instances[i], max_instance[i]);
        strcat(line_to_print, final_print);
    }
    return line_to_print;
}

void sigchld_handler(int sig){
    pid_t pid;
    int status, n_cmds;
    int index_task;
    char** parsed_task;
    char* filter;
    pid = waitpid(-1, &status, WNOHANG);
    index_task = pid_index(pid);
    parsed_task = cmd_parser(strdup(tasks[index_task]), &n_cmds);
    for (int i = 6 ; i < n_cmds ; i++){
        filter = filterWithName(parsed_task[i]);
        open_instances[searchFilterIndex(filter)]--;
    }
    tasks[index_task] = "acabou";
    flag_block = TRUE;
    kill(atoi(parsed_task[2]), SIGKILL);
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

void read_config(char** names, char** filters, int* max_instance, char* config_filename, char* filters_folder){
    char* line = malloc(sizeof(char)*100);
    char** parsed_line;
    int counter = 0;
    int ignore;
    char* folder_cat;
    int fd = open(config_filename, O_RDONLY, 0666);
    ssize_t size;
    while ((size = readln(fd, line, 100)) > 0){
        folder_cat = strdup(filters_folder);
        line[size] = '\0';
        parsed_line = cmd_parser(line, &ignore);
        names[counter] = strdup(parsed_line[0]);
        strcat(folder_cat, strdup(parsed_line[1]));
        filters[counter] = folder_cat;
        max_instance[counter] = atoi(parsed_line[2]);
        counter++;
    }
    n_filters = counter;
}


void new_executeTask(int number_filters, char **f_a_executar, char* input_file, char* output_file){
    int input = open(strdup(input_file), O_RDONLY, 0666);
    char * tmp_out_buf = malloc(sizeof(char) * 128);
    char* buffer = malloc(sizeof(char) * MAX_SIZE_BUFFER); 
    sprintf(tmp_out_buf,"../tmp/task$%d",number_of_tasks);
    int tmp_output = open(tmp_out_buf, O_CREAT | O_WRONLY, 0666);
    int fd_pipe[number_filters + 1][2],nb_read;
    pipe(fd_pipe[0]); //pipe para o pai escrever para o primeiro filtro o conteudo do ficheiro input
    int counter;
    for(counter = 0; counter < number_filters ; counter++){
        pipe(fd_pipe[counter+1]);//criação do pipe para a comunicação entre o filtro que vai executar e o proximo filtro
        if(counter == 0 ){
            switch(fork()){
                case 0:
                    /* -- fechar o descritor de escrita do pipe -- */
                    close(fd_pipe[counter][1]);
                    dup2(fd_pipe[counter][0],STDIN_FILENO);
                    close(fd_pipe[counter][0]);
                    dup2(fd_pipe[counter+1][1],STDOUT_FILENO);
                    close(fd_pipe[counter+1][1]);
                    execl(f_a_executar[counter],f_a_executar[counter],NULL);
                    break;
                default:
                    close(fd_pipe[counter+1][1]);
                    /* --- ler o conteudo do ficheiro --- */
                    while((nb_read = read(input,buffer,MAX_SIZE_BUFFER)) > 0){
                        write(fd_pipe[counter][1],buffer,nb_read); /* -- escrever para o pipe o conteudo do ficheiro -- */
                    }
                    close(fd_pipe[counter][1]);
                    break;
            }
        }
        else{
            switch(fork()){
                case 0:
                    dup2(fd_pipe[counter][0],STDIN_FILENO);
                    close(fd_pipe[counter][0]);
                    dup2(fd_pipe[counter+1][1],STDOUT_FILENO);
                    close(fd_pipe[counter+1][1]);
                    execl(f_a_executar[counter],f_a_executar[counter],NULL);
                    break;
                default:
                    close(fd_pipe[counter+1][1]);
                    break;
            }
        }
    }
    while((nb_read = read(fd_pipe[counter][0],buffer,MAX_SIZE_BUFFER)) > 0){
        write(tmp_output,buffer,nb_read);
    }
    if(fork() == 0){
        execlp("ffmpeg", "ffmpeg" ,"-i" ,tmp_out_buf, output_file, NULL);
        perror("execlp");
    }
    else wait(NULL);
}


int main(int argc, char* argv[]){
    /* ------ make named pipe ------ */
    mkfifo("../tmp/client_server",0666);
    mkfifo("../tmp/server_client",0666);
    mkfifo("../tmp/status",0666);
    int client_server_rd = open("../tmp/client_server",O_RDONLY,0666); 
    int client_server_wr = open("../tmp/client_server",O_WRONLY,0666); 
    int server_client_wr = open("../tmp/server_client",O_WRONLY,0666);
    int status_rd = open("../tmp/status",O_RDONLY,0666);
    pid_t pid;
    for (int i = 0 ; i < 5 ; i++){
        open_instances[i] = 0;
    }

    read_config(names, filters, max_instance, argv[1], argv[2]);

    

    char** f_a_executar;
    
    char* status = malloc(sizeof(char) * STD_SIZE);
    char **parsed_commands;
    int n_commands;
    char* task_to_be_parsed = malloc(sizeof(char)*STD_SIZE);
    int filters_exec;
    int index_task;
    int read_value;
    signal(SIGCHLD, sigchld_handler);
    while (1){
        if((read_value = read(status_rd, status, STD_SIZE)) > 0){
            char* string_to_print = printStatus(n_filters);
            write(server_client_wr, string_to_print, STD_SIZE);
        }
        if ( flag_block && current_waiting < next_waiting){ //verificar se há bloqueados
            write(STDOUT_FILENO,"Analisando Tasks Bloqueadas\n",29);
            sprintf (task_to_be_parsed, "%s", waiting_tasks[current_waiting]);

            parsed_commands = cmd_parser(strdup(task_to_be_parsed), &n_commands);
            filters_exec = n_commands-6;
            index_task = searchTaskIndex(task_to_be_parsed);
            f_a_executar = malloc(sizeof(char*) * (filters_exec));

            for (int i = 6; i < n_commands; i++){
                f_a_executar[i-6] = malloc(sizeof(char)*NAME_FILTER_SIZE);
                sprintf (f_a_executar[i-6], "%s", filterWithName(parsed_commands[i]));
            }
            

            
            if (checkFilters(f_a_executar, filters_exec)){ //verificar se podem executar
                kill(atoi(parsed_commands[2]), SIGUSR1);    // envia sinal ao cliente a dizer que está a ser executado
                if((pid = fork()) == 0){
                    write (STDOUT_FILENO, "Started task\n", 14);
                    signal(SIGCHLD,SIG_DFL);
                    new_executeTask(filters_exec, f_a_executar, parsed_commands[4], parsed_commands[5]);
                    exit(0);
                }
                else{
                    for(int i = 0; i < filters_exec; i++){
                        open_instances[searchFilterIndex(f_a_executar[i])]++;
                    }
                    pids[index_task] = pid;
                }
                current_waiting++;
            }
            else{ // se não puder, vai para a lista de bloqueados
                write(STDOUT_FILENO, "Task bloqueada ainda não pode executar\n", 40);
                flag_block = FALSE;
                //pause();

            }
        }
        else if (flag_block){ // se não houver bloqueados, ler próxima tarefa do pipe

            char* task = malloc(sizeof(char) * STD_SIZE);
            printf("\nLeitura da Task\n");
            read(client_server_rd,task,STD_SIZE);
            printf("Task submetida: %s\n",task);

            if (strcmp(task, "ignore_this_message") != 0){
                parsed_commands = cmd_parser(strdup(task), &n_commands);
                filters_exec = n_commands-4;
                
                f_a_executar = malloc(sizeof(char*) * (filters_exec));

                for (int i = 4; i < n_commands; i++){
                    f_a_executar[i-4] = malloc(sizeof(char) * NAME_FILTER_SIZE);
                    sprintf (f_a_executar[i-4], "%s", filterWithName(parsed_commands[i]));
                }



                tasks[number_of_tasks] = malloc(sizeof(char) * STD_SIZE);
                sprintf (tasks[number_of_tasks], "task #%d: %s", number_of_tasks+1, task);
                if (checkFilters(f_a_executar, filters_exec)){
                    kill(atoi(parsed_commands[0]), SIGUSR1);    // envia sinal ao cliente a dizer que está a ser executado
                    if((pid = fork()) == 0){
                        write (STDOUT_FILENO, "Started task\n", 14);
                        signal(SIGCHLD,SIG_DFL);
                        new_executeTask(filters_exec, f_a_executar, parsed_commands[2], parsed_commands[3]);
                        exit(0);
                    }
                    else{
                        for(int i  = 0; i < filters_exec; i++){
                            open_instances[searchFilterIndex(f_a_executar[i])]++;
                        }
                        pids[number_of_tasks] = pid;
                    }
                }
                else{
                    waiting_tasks[next_waiting] = strdup(tasks[number_of_tasks]);
                    kill(atoi(parsed_commands[0]), SIGUSR2);
                    next_waiting++;
                    write(STDOUT_FILENO, "Task bloqueada\n", 16);
                }
                number_of_tasks++;
            }

            
        }
    }

    close (client_server_rd);
    close (client_server_wr);
    close(server_client_wr);
    close(status_rd);
    unlink("../tmp/client_server");
    unlink("../tmp/server_client");
    unlink("../tmp/status");
    
    return 0;
}