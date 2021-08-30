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