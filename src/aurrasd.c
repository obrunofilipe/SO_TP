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

char * line ;
int line_index = 0;
int line_end = 0;



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

int main(int argc, char* argv[]){
    
    int n_filters = n_lines_config();

    char** names = malloc(sizeof(char*)*n_filters);
    char** filters = malloc(sizeof(char*)*n_filters);
    int* max_instance = malloc(sizeof(int)*n_filters);
    read_config(names, filters, max_instance);
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