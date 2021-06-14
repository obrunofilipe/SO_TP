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


/*---------- Variaveis Globais ----------*/

int n_filters;
char *names[5];
char *filters[5];
int max_instance[5];
int open_instances[5];
char *tasks[1024];
char* tasks_final[1024];
pid_t pids[1024];
//pid_t client_pids[1024];
char *waiting_tasks[512];
int current_waiting = 0;
int next_waiting = 0;
int nextTask;
int ongoing_tasks = 0;
int current_task = 0;
int number_of_tasks = 0;

int server_client;


int searchFilterIndex(char* filterName);

int checkFilters(char** f_to_exec, int number_f){
    int index;
    int res = FALSE;
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
    char* filter = malloc(sizeof(char)*30);
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
            printf ("Task %d: %s\n", i, tasks[i]);
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

// transform /home/user/samples/sample-1.m4a output-1.m4a baixo rapido 
// filtros a partir do 3 
// pid task #n transform input output f1
void sigchld_handler(int sig){
    pid_t pid;
    int status, n_cmds;
    int index_task;
    char** parsed_task;
    int i = 0;
    char* filter;
    pid = waitpid(-1, &status, WNOHANG);
    index_task = pid_index(pid);
    parsed_task = cmd_parser(strdup(tasks[index_task]), &n_cmds);
    printf ("TASK NO HANDLER: %s\n", tasks[index_task]);
    for (int i = 6 ; i < n_cmds ; i++){
        filter = filterWithName(parsed_task[i]);
        open_instances[searchFilterIndex(filter)]--;
    }
    for (int i = 0; i < 5; i++){
        printf ("%d, ", open_instances[i]);
    }
    printf ("\n");
    tasks[index_task] = "acabou";
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
/*
void executeTask(int number_filters, char **f_a_executar, char* input_file, char* output_file){
    int fp [number_filters + 1][2];
    int current_pipe = 0;
    char *buffer = malloc(sizeof(char) * 1024);
    buffer = strdup("\nmensagem para teste\n");
    int filter = 0;
    int nb_read;
    int ft = open(input_file,O_RDONLY,0666);
    if(ft != -1 ) printf("\nficheiro aberto\n");
    int ftf = open("./tmp/coiso", O_CREAT | O_WRONLY, 0666);
    int status,counter = 0;
    pipe(fp[current_pipe]);
    int pid;
    switch (fork()) {
        case 0: //processo filho de controlo
            write(STDOUT_FILENO,"\n\n\nENTROU NO FILHO \n\n\n\n",30);
            close(fp[current_pipe][1]);
            while(current_pipe < number_filters){
                
                write(STDOUT_FILENO,"\na executar filtros\n",21);
                pipe(fp[current_pipe+1]);
                switch (pid = fork()){
                    case 0:
                        
                        while((nb_read = read(fp[current_pipe][0],buffer,1024)) > 0){
                            write(fp[current_pipe+1][1],buffer,nb_read);
                        }
                       
                        printf ("A executar o filtro %s\n", f_a_executar[filter]);
                        dup2(fp[current_pipe][0],STDIN_FILENO);
                        close(fp[current_pipe][0]);
                        dup2(fp[current_pipe+1][1],STDOUT_FILENO);
                        close(fp[current_pipe+1][1]);
                        execl(f_a_executar[filter], f_a_executar[filter], NULL);
                        printf ("nao executou o filtro %s\n", f_a_executar[filter]);
                        break;
                    default:
                        close(fp[current_pipe][0]);   //fechar o descritor de leitura do pipe anterior para nao ser herdado por mais nenhum processo
                        close(fp[current_pipe+1][1]);
                        break;
                }
                current_pipe++;
                filter++;
                
            }
            write(STDOUT_FILENO, "\n\nprocesso filho vai ler do pipe para escrever no ficheiro\n", 62);
            while((nb_read = read(fp[current_pipe][0],buffer,1024)) > 0){
                write(ftf,buffer,nb_read);
                write(STDOUT_FILENO,"escrever\n",10);
            }

            exit(0);
            break;
        default: //processo pai:
            write(STDOUT_FILENO,"\n\nprocesso pai vai escrever no pipe\n",35);
            while ((nb_read = read(ft, buffer, 1024)) > 0) {
                write(fp[current_pipe][1],buffer,nb_read);
            }
            close(fp[current_pipe][1]);
            pause();
            break;
    }
}
*/
void new_executeTask(int number_filters, char **f_a_executar, char* input_file, char* output_file){
    int input = open(input_file, O_RDONLY, 0666);
    char * tmp_out_buf = malloc(sizeof(char) * 128);
    char* buffer =malloc(sizeof(char) * MAX_SIZE_BUFFER); 
    sprintf(tmp_out_buf,"tmp/task$%d",number_of_tasks);
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
                        printf("escreveu\n");
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
        //close(STDOUT_FILENO);
        printf("ficheiro: %s\n",output_file);
        execlp("ffmpeg", "ffmpeg" ,"-i" ,tmp_out_buf, output_file, NULL);
        perror("execlp");
    }
    else wait(NULL);
}


int main(int argc, char* argv[]){
    /* ------ make named pipe ------ */
    mkfifo("./tmp/client_server",0666);
    mkfifo("./tmp/server_client",0666);
    mkfifo("./tmp/status",0666);
    int client_server_rd = open("./tmp/client_server",O_RDONLY,0666); 
    int client_server_wr = open("./tmp/client_server",O_WRONLY,0666); 
    int server_client_wr = open("./tmp/server_client",O_WRONLY,0666);
    int status_rd = open("./tmp/status",O_RDONLY,0666);
    pid_t pid;
    for (int i = 0 ; i < 5 ; i++){
        open_instances[i] = 0;
    }
    read_config(names, filters, max_instance, argv[1], argv[2]);

    for (int i = 0; i < n_filters; i++){
        printf ("%s\n", filters[i]);
        printf ("%s\n", names[i]);
    }

    char** f_a_executar;
    
    char* status = malloc(sizeof(char) * 10);
    char **parsed_commands;
    //char task_array_test[4][512] = {"transform /home/user/samples/sample-1.m4a output-1.m4a baixo rapido ", "transform /home/user/samples/sample-1.m4a output-1.m4a baixo rapido eco lento alto", "transform /home/user/samples/sample-2.m4a output-3.m4a rapido rapido baixo eco", "transform /home/user/samples/sample-2.m4a output-3.m4a lento lento baixo eco"};
    int n_commands;
    char* task_to_be_parsed = malloc(sizeof(char)*256);
    int filters_exec;
    int task_counter = 1;
    int index_task;
    int read_value;
    signal(SIGCHLD, sigchld_handler);
    while (1){
        if((read_value = read(status_rd, status, 10)) > 0 /*&& strcmp(status, "status") == 0*/){
            printf ("Number of tasks: %d\n", number_of_tasks);
            char* string_to_print = printStatus(n_filters);
            write(server_client_wr, string_to_print, strlen(string_to_print));
        }
        printf ("%d\n", read_value);
        if (current_waiting < next_waiting){ //verificar se há bloqueados
            printf ("Current waiting: %d\n", current_waiting);
            printf ("Next waiting: %d\n", next_waiting);
            printf ("A analisar um processo bloqueado\n");
            sprintf (task_to_be_parsed, "%s", waiting_tasks[current_waiting]); // "task #n_task: pid transform input output f1 f2 f3..."

            parsed_commands = cmd_parser(strdup(task_to_be_parsed), &n_commands);
            filters_exec = n_commands-6;
            index_task = searchTaskIndex(task_to_be_parsed);
            f_a_executar = malloc(sizeof(char*) * (filters_exec));

            for (int i = 6; i < n_commands; i++){
                f_a_executar[i-6] = malloc(sizeof(char)*10);
                sprintf (f_a_executar[i-6], "%s", filterWithName(parsed_commands[i]));
            }
            
            
            
            if (checkFilters(f_a_executar, filters_exec)){ //verificar se podem executar
                printf ("passou check filters bloqueados!!!!!\n");
                printf ("Task: %s | pid: %d\n", waiting_tasks[current_waiting], atoi(parsed_commands[2]));
                kill(atoi(parsed_commands[2]), SIGUSR1);    // envia sinal ao cliente a dizer que está a ser executado
                if((pid = fork()) == 0){
                    write (STDOUT_FILENO, "Started task\n", 14);
                    signal(SIGCHLD,SIG_DFL);
                    new_executeTask(filters_exec, f_a_executar, parsed_commands[4], parsed_commands[5]);
                    exit(0);
                }
                else{
                    char* filter;
                    for(int i = 0; i < filters_exec; i++){
                        open_instances[searchFilterIndex(f_a_executar[i])]++;
                    }
                    pids[index_task] = pid;
                }
                current_waiting++;
            }
            else{ // se não puder, vai para a lista de bloqueados
                printf ("Não passou o checkFilters bloqueados\n");
                printf ("Task bloqueada ainda não pode executar\n");
                printf ("Estou no pause\n");
                //waiting_tasks[next_waiting] = strdup(tasks[current_task]);
                //kill(atoi(parsed_commands[2]), SIGUSR2); // envia sinal ao cliente a dizer que está à espera para ser executado
                //next_waiting++;
                pause();
                printf ("Vou sair do pause\n");
            }
        }
        else{ // se não houver bloqueados, ler próxima tarefa do pipe

            char* task = malloc(sizeof(char)*1024);
            printf("\nleitura\n");
            read(client_server_rd,task,512);
            printf("%s\n",task);
            for (int i = 0; i < 5; i++){
                printf ("%d,", open_instances[i]);
            }
            printf ("\n");
            for (int i = 0; i < 5; i++){
                printf ("%d,", max_instance[i]);
            }

            if (strcmp(task, "ignore_this_message") != 0){
                parsed_commands = cmd_parser(strdup(task), &n_commands);
                printf ("parsed: %s\n", parsed_commands[0]);
                filters_exec = n_commands-4;
                
                f_a_executar = malloc(sizeof(char*) * (filters_exec));

                for (int i = 4; i < n_commands; i++){
                    f_a_executar[i-4] = malloc(sizeof(char)*30);
                    sprintf (f_a_executar[i-4], "%s", filterWithName(parsed_commands[i]));
                }



                tasks[number_of_tasks] = malloc(sizeof(char) * 512);
                sprintf (tasks[number_of_tasks], "task #%d: %s", number_of_tasks+1, task);
                
                printf ("%s\n", task);
                printf ("%s\n", tasks[number_of_tasks]);
                if (checkFilters(f_a_executar, filters_exec)){
                    printf("passou chsck filters nao bloqueados\n");
                    kill(atoi(parsed_commands[0]), SIGUSR1);    // envia sinal ao cliente a dizer que está a ser executado
                    if((pid = fork()) == 0){
                        write (STDOUT_FILENO, "Started task\n", 14);
                        signal(SIGCHLD,SIG_DFL);
                        new_executeTask(filters_exec, f_a_executar, parsed_commands[2], parsed_commands[3]);
                        exit(0);
                    }
                    else{
                        char* filter;
                        for(int i  = 0; i < filters_exec; i++){
                            open_instances[searchFilterIndex(f_a_executar[i])]++;
                        }
                        pids[number_of_tasks] = pid;
                    }
                }
                else{
                    printf ("não passou check filters nao bloqueados\n");
                    waiting_tasks[next_waiting] = strdup(tasks[number_of_tasks]);
                    kill(atoi(parsed_commands[0]), SIGUSR2);
                    next_waiting++;
                    printf ("Task bloqueada\n");
                }
                number_of_tasks++;
            }

            
        }
    }

    //close (fdw);
    //close (fdr);
    //unlink("fifo");


   //while(wait(&status));
    //while(1);
    return 0;
}