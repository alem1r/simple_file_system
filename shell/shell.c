#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <simplefs.h>

#define MAX_COMMAND_LENGTH 256
#define MAX_ARGUMENTS_NUM  2
#define MAX_INPUT_SIZE     2048

// GLOBAL VARIABLES
DiskDriver disk;
SimpleFS fs;
DirectoryHandle* current_dir = NULL;
char command[MAX_COMMAND_LENGTH];

// COMMANDS

void format(int argc, char* argv[MAX_ARGUMENTS_NUM + 1]) {

    if (argc != 1) {
        printf("Usage: format\n");
        return;
    }
    SimpleFS_format(&fs);
    SimpleFS_closeDir(current_dir);
    current_dir = SimpleFS_init(&fs, &disk);
}

/*
 * empty dir
 */
void mkdir(int argc, char* argv[MAX_ARGUMENTS_NUM + 1]) {

    if (argc != 2) {
        printf("Usage: mkdir <dirname>\n");
        return;
    }

    int ret = SimpleFS_mkDir(current_dir, argv[1]);
    if (ret == -1) 
        fprintf(stderr, "An error occurred in creating new directory.\n");
}

// print the content of the file

void cat(int argc, char* argv[MAX_ARGUMENTS_NUM + 1]) {

    if (argc != 2) {
        printf("Usage: cat <filename>\n");
        return;
    }
    FileHandle* fh = SimpleFS_openFile(current_dir, argv[1]);
    if (fh == NULL) {
        fprintf(stderr, "An error occurred in opening file.\n");
        return;
    }

    int file_size = fh->fcb->fcb.size_in_bytes;
    char* str = calloc(file_size + 1, sizeof(char));
    int ret = SimpleFS_read(fh, str, file_size);
    if (ret == -1) {
        fprintf(stderr, "An error occurred in reading from file.\n");
        free(str);
        SimpleFS_closeFile(fh);
        return;
    }

    printf("%s\n", str);

    free(str);
    SimpleFS_closeFile(fh);
}

void touch(int argc, char* argv[MAX_ARGUMENTS_NUM + 1]) {

    if (argc != 2) {
        printf("Usage: touch <filename>\n");
        return;
    }

    int ret = SimpleFS_createFile(current_dir, argv[1]);
    if (ret == -1) 
        fprintf(stderr, "An error occurred in creating new file.\n");
}

void cd(int argc, char* argv[MAX_ARGUMENTS_NUM + 1]) {

    if (argc != 2) {
        printf("Usage: cd <dirname>\n");
        return;
    }

    int ret = SimpleFS_changeDir(current_dir, argv[1]);
    if (ret == -1) 
        fprintf(stderr, "An error occurred in changing directory.\n");
}

/*
 * List the content.
 */
void ls(int argc, char* argv[MAX_ARGUMENTS_NUM + 1]) {

    if (argc != 1) {
        printf("Usage: ls\n");
        return;
    }

    int i;
    char** names = calloc(current_dir->dcb->num_entries, sizeof(char*));
    int ret = SimpleFS_readDir(names, current_dir);
    if (ret == -1) {
        fprintf(stderr, "An error occurred while listing files and dirs.\n");
        for (i = 0; i < current_dir->dcb->num_entries; i++) 
            free(names[i]);   
        free(names);
        return;
    }

    for (i = 0; i < current_dir->dcb->num_entries; i++) {
        
        FileHandle* something = SimpleFS_openFile(current_dir, names[i]);
        if (something == NULL) printf("dir: ");
        else { 
            printf("file: ");
            SimpleFS_closeFile(something);
        }
        
        printf("%s\n", names[i]);
    }

    for (i = 0; i < current_dir->dcb->num_entries; i++) 
        free(names[i]);   
    free(names);
}


void help(int argc, char* argv[MAX_ARGUMENTS_NUM + 1]) {
    
    if (argc != 1) {
        printf("Usage: help\n");
        return;
    }
    printf("Available commands:\n");
    printf("format: formats the disk.\n");
    printf("mkdir: create a new directory in the current one.\n");
    printf("write: enables to write into an existing file.\n");
    printf("cat: prints out the content of an existing file.\n");
    printf("touch: create a new empty file in the current directory.\n");
    printf("cd: change the current directory.\n");
    printf("ls: list all the files in the current directory.\n");
    printf("help: command inception.\n");
    printf("exit: exit the shell.\n");
}


void do_command_loop(void) {

    do {
        char* argv[MAX_ARGUMENTS_NUM + 1] = {NULL};

        printf("%s> ", current_dir->dcb->fcb.name);
        fgets(command, MAX_COMMAND_LENGTH, stdin);
        
        argv[0] = strtok(command, " ");
        argv[1] = strtok(NULL, "\n");
        int argc = (argv[1] == NULL) ? 1 : 2;
        if (argv[1] == NULL) 
            argv[0][strlen(argv[0]) - 1] = 0x00;

        if (strcmp(argv[0], "format") == 0) {
            format(argc, argv);
        }
        else if (strcmp(argv[0], "mkdir") == 0) {
            mkdir(argc, argv); 
        }
        else if (strcmp(argv[0], "write") == 0) {
            write(argc, argv); 
        }
        else if (strcmp(argv[0], "cat") == 0) {
            cat(argc, argv); 
        }
        else if (strcmp(argv[0], "touch") == 0) {
            touch(argc, argv); 
        }
        else if (strcmp(argv[0], "cd") == 0) {
            cd(argc, argv); 
        }
        else if (strcmp(argv[0], "ls") == 0) {
            ls(argc, argv); 
        }
       
        
        else if (strcmp(argv[0], "help") == 0) {
            help(argc, argv); 
        }
        else if (strcmp(argv[0], "exit") == 0) {
            printf("Finished!\n");
            SimpleFS_closeDir(current_dir);
            exit(EXIT_SUCCESS);
        }
        else {
            printf("Invalid command\n");
        }
    } while(1);
}


int main(int argc, char * argv[]) {
 
    DiskDriver_init(&disk, "prova", 2048);
    
    current_dir = SimpleFS_init(&fs, &disk);
    if (current_dir == NULL) {
        fprintf(stderr, "[ERROR] Cannot initialize file system.\n");
        exit(EXIT_FAILURE);
    }

    printf("Welcome, run 'help' to see available commands!\n");
    do_command_loop();
    return 0;
}