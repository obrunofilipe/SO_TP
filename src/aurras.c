#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>



int main(int argc, char* argv[]){

    char* commands = malloc(sizeof(char) * 1024);
    
    for (int i = 1; i < argc; i++){
        strcat(commands, argv[i]);
        strcat(commands, " ");
    }
    
    char info[95] = "./aurras status \n./aurras transform input-filename output-filename filter-id-1 filter-id-2 ...\n";
    int fdw = open("fifo", O_WRONLY);
    if (argc == 1){
        write (fdw, info, 95);
    }
    else{
        write (fdw, commands, 120);
    }

    return 0;
}