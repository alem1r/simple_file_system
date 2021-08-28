#include <simplefs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int max_entries_db = (BLOCK_SIZE - sizeof(BlockHeader)) / sizeof(int);
const int max_entries_fdb = (BLOCK_SIZE - sizeof(BlockHeader) -
                             sizeof(FileControlBlock) - sizeof(int)) / sizeof(int);
const int max_data_ffb = BLOCK_SIZE - sizeof(BlockHeader) -
                           sizeof(FileControlBlock);
const int max_data_fb = BLOCK_SIZE - sizeof(BlockHeader);

static int SimpleFS_exists(DirectoryHandle* d, const char* filename) {
    
    FirstDirectoryBlock* fdb = d->dcb;
    int entries = fdb->num_entries;
    int idx, ret;

    for (idx = 0; idx < max_entries_fdb; idx++) {
        if (idx >= entries)
            return 0;
        
        FirstFileBlock ffb;
        int block_num = fdb->file_blocks[idx];
        ret = DiskDriver_readBlock(d->sfs->disk, &ffb, block_num);
        if (ret == -1)
            return 0;
        
        if (strncmp(ffb.fcb.name, filename, 128) == 0)
            return ffb.header.block_in_disk;
    }

    entries -= idx;

    if (entries == 0)
        return 0;

    DirectoryBlock db;
    ret = DiskDriver_readBlock(d->sfs->disk, &db, fdb->header.next_block);
    if (ret == -1) {
        if (DEBUG) printf("[SFS - exists] Cannot read from disk.\n");
        return 0;
    }
    for (idx = 0; idx < max_entries_db; idx++) {
        if (idx >= entries)
            return 0;

        FirstFileBlock ffb;
        int block_num = db.file_blocks[idx];
        ret = DiskDriver_readBlock(d->sfs->disk, &ffb, block_num);
        if (ret == -1)
            return 0;
        
        if (strncmp(ffb.fcb.name, filename, 128) == 0)
            return ffb.header.block_in_disk;
    }
    entries -= idx;
    while (db.header.next_block != -1) {
        ret = DiskDriver_readBlock(d->sfs->disk, &db, db.header.next_block);
        if (ret == -1) {
            if (DEBUG) printf("[SFS - exists] Cannot read from disk.\n");
            return 0;
        }
        for (idx = 0; idx < max_entries_db; idx++) {
            if (idx >= entries)
                return 0;

            FirstFileBlock ffb;
            int block_num = db.file_blocks[idx];
            ret = DiskDriver_readBlock(d->sfs->disk, &ffb, block_num);
            if (ret == -1)
                return 0;

            if (strncmp(ffb.fcb.name, filename, 128) == 0)
                return ffb.header.block_in_disk;
        }
        entries -= idx;
    }
    return 0;
}

static int SimpleFS_removeDirBlock(DirectoryHandle* d, DirectoryBlock* b) {
    if (b->header.next_block == -1) {
        return DiskDriver_freeBlock(d->sfs->disk, b->header.block_in_disk);
    }
    else {
        DirectoryBlock db = {0};
        int ret = DiskDriver_readBlock(d->sfs->disk, &db, b->header.next_block);
        if (ret == -1) {
            if (DEBUG) printf("[SFS - removeDirBlock] Cannot read from disk.\n");
            return -1;
        }

        SimpleFS_removeDirBlock(d, &db);
        return DiskDriver_freeBlock(d->sfs->disk, b->header.block_in_disk);
    }
}

static int SimpleFS_removeFileBlock(DirectoryHandle* d, FileBlock* b) {
    if (b->header.next_block == -1) {
        return DiskDriver_freeBlock(d->sfs->disk, b->header.block_in_disk);
    }
    else {
        FileBlock fb = {0};
        int ret = DiskDriver_readBlock(d->sfs->disk, &fb, b->header.next_block);
        if (ret == -1) {
            if (DEBUG) printf("[SFS - removeFileBlock] Cannot read from disk.\n");
            return -1;
        }

        SimpleFS_removeFileBlock(d, &fb);
        return DiskDriver_freeBlock(d->sfs->disk, b->header.block_in_disk);
    }
}





int SimpleFS_createFile(DirectoryHandle* d, const char* filename) {

    if (SimpleFS_exists(d, filename)) {
        if (DEBUG) printf("[SFS - createFile] File already exists.\n");
        return -1;
    }

    FirstDirectoryBlock* fdb = d->dcb;

    int ret;
    int free_block = DiskDriver_getFreeBlock(d->sfs->disk, 0);
    if (free_block == -1) {
        if (DEBUG) printf("[SFS - createFile] No free block.\n");
        return -1;
    }

    FirstFileBlock ffb = {0};
    ffb.header.previous_block = -1;
    ffb.header.next_block = -1;
    ffb.header.block_in_file = 0;
    ffb.header.block_in_disk = free_block;

    ffb.fcb.directory_block = fdb->header.block_in_disk;
    ffb.fcb.size_in_bytes = 0;
    ffb.fcb.size_in_blocks = 1;
    ffb.fcb.is_dir = 0;
    ffb.fcb.idx_in_directory = fdb->num_entries;
    strncpy(ffb.fcb.name, filename, 128);

    ret = DiskDriver_writeBlock(d->sfs->disk, &ffb, free_block);
    if (ret == -1) {            
        if (DEBUG) printf("[SFS - createFile] Cannot write on disk.\n");
        return -1;
    }

    if (fdb->num_entries < max_entries_fdb) {
        fdb->file_blocks[fdb->num_entries] = free_block;
        fdb->num_entries += 1;
        
        ret = DiskDriver_writeBlock(d->sfs->disk, fdb, fdb->header.block_in_disk);
        if (ret == -1) {
            if (DEBUG) printf("[SFS - createFile] Cannot write on disk.\n");
            return -1; 
        }
    }
    else {
        int entries = fdb->num_entries - max_entries_fdb;
        if (entries == 0 || entries % max_entries_db == 0) {
            int block_free_block = DiskDriver_getFreeBlock(d->sfs->disk, 0);
            if (block_free_block == -1) {
                if (DEBUG) printf("[SFS - createFile] No free block.\n");
                return -1;
            }

            DirectoryBlock new_block = {0};
            new_block.header.next_block = -1;
            new_block.header.block_in_disk = block_free_block;

            if (entries == 0) {
                new_block.header.block_in_file = fdb->header.block_in_file + 1;
                new_block.header.previous_block = fdb->header.block_in_disk;
                new_block.file_blocks[0] = free_block;
                fdb->header.next_block = block_free_block;
            }
            else {
                DirectoryBlock last_block;
                ret = DiskDriver_readBlock(d->sfs->disk, &last_block, fdb->header.next_block);
                if (ret == -1) {
                    if (DEBUG) printf("[SFS - createFile] Cannot read from disk.\n");
                    return -1;
                }
                while (last_block.header.next_block != -1) {
                    ret = DiskDriver_readBlock(d->sfs->disk, &last_block, last_block.header.next_block);
                    if (ret == -1) {
                        if (DEBUG) printf("[SFS - createFile] Cannot read from disk.\n");
                        return -1;
                    }
                }

                new_block.header.block_in_file = last_block.header.block_in_file + 1;
                new_block.header.previous_block = last_block.header.block_in_disk;
                new_block.file_blocks[0] = free_block;
                last_block.header.next_block = block_free_block;

                ret = DiskDriver_writeBlock(d->sfs->disk, &last_block, last_block.header.block_in_disk);
                if (ret == -1) {
                    if (DEBUG) printf("[SFS - createFile] Cannot write on disk.\n");
                    return -1; 
                }
            }
            ret = DiskDriver_writeBlock(d->sfs->disk, &new_block, new_block.header.block_in_disk);
            if (ret == -1) {
                if (DEBUG) printf("[SFS - createFile] Cannot write on disk.\n");
                return -1; 
            }
        }
        else {
            DirectoryBlock db;
            ret = DiskDriver_readBlock(d->sfs->disk, &db, fdb->header.next_block);
            if (ret == -1) {
                if (DEBUG) printf("[SFS - createFile] Cannot read from disk.\n");
                return -1;
            }
            while (db.header.next_block != -1) {
                ret = DiskDriver_readBlock(d->sfs->disk, &db, db.header.next_block);
                if (ret == -1) {
                    if (DEBUG) printf("[SFS - createFile] Cannot read from disk.\n");
                    return -1;
                }
            }

            db.file_blocks[entries % max_entries_db] = free_block;
            ret = DiskDriver_writeBlock(d->sfs->disk, &db, db.header.block_in_disk);
            if (ret == -1) {
                if (DEBUG) printf("[SFS - createFile] Cannot write on disk.\n");
                return -1; 
            }
        }
        fdb->num_entries += 1;
        fdb->fcb.size_in_bytes += BLOCK_SIZE;
        fdb->fcb.size_in_blocks += 1;

        ret = DiskDriver_writeBlock(d->sfs->disk, fdb, fdb->header.block_in_disk);
        if (ret == -1) {
            if (DEBUG) printf("[SFS - createFile] Cannot write on disk.\n");
            return -1; 
        }
    }
    return 0;
}
int SimpleFS_readDir(char** names, DirectoryHandle* d) {
 
    FirstDirectoryBlock* fdb = d->dcb;
    int entries = fdb->num_entries;
    int idx, ret;

    for (idx = 0; idx < max_entries_fdb; idx++) {
        if (idx >= entries)
            return 0;
        
        FirstFileBlock ffb;
        int block_num = fdb->file_blocks[idx];
        ret = DiskDriver_readBlock(d->sfs->disk, &ffb, block_num);
        if (ret == -1)
            return -1;

        names[idx] = strndup(ffb.fcb.name, 128);
    }

    entries -= idx;
    DirectoryBlock db;
    ret = DiskDriver_readBlock(d->sfs->disk, &db, fdb->header.next_block);
    if (ret == -1) {
        if (DEBUG) printf("[SFS - createFile] Cannot read from disk.\n");
        return 0;
    }
    for (idx = 0; idx < max_entries_db; idx++) {
        if (idx >= entries)
            return 0;

        FirstFileBlock ffb;
        int block_num = db.file_blocks[idx];
        ret = DiskDriver_readBlock(d->sfs->disk, &ffb, block_num);
        if (ret == -1)
            return -1;
        
        int names_idx = max_entries_fdb + idx + max_entries_db * (db.header.block_in_file - 1); 
        names[names_idx] = strndup(ffb.fcb.name, 128); 
    }
    entries -= idx;
    while (db.header.next_block != -1) {
        ret = DiskDriver_readBlock(d->sfs->disk, &db, db.header.next_block);
        if (ret == -1) {
            if (DEBUG) printf("[SFS - createFile] Cannot read from disk.\n");
            return 0;
        }
        for (idx = 0; idx < max_entries_db; idx++) {
            if (idx >= entries)
                return 0;

            FirstFileBlock ffb;
            int block_num = db.file_blocks[idx];
            ret = DiskDriver_readBlock(d->sfs->disk, &ffb, block_num);
            if (ret == -1)
                return -1;

            int names_idx = max_entries_fdb + idx + max_entries_db * (db.header.block_in_file - 1); 
            names[names_idx] = strndup(ffb.fcb.name, 128); 
        }
        entries -= idx;
    }
    return 0;
}

int SimpleFS_closeDir(DirectoryHandle* d) {
    if (d->directory != NULL)
        free(d->directory);
    free(d->dcb);
    free(d);
    return 0;
}

FileHandle* SimpleFS_openFile(DirectoryHandle* d, const char* filename) {

    int block_num = SimpleFS_exists(d, filename);
    if (block_num == 0) {
        if (DEBUG) printf("[SFS - openFile] File doesn't exists.\n");
        return NULL;
    }

    FirstFileBlock* ffb = calloc(1, sizeof(FirstFileBlock));
    int ret = DiskDriver_readBlock(d->sfs->disk, ffb, block_num);
    if (ret == -1) {
        if (DEBUG) printf("[SFS - openFile] Cannot read from disk.\n");
        free(ffb);
        return NULL;
    }

    if (ffb->fcb.is_dir == 1) {
        if (DEBUG) printf("[SFS - openFile] Cannot open a directory.\n");
        free(ffb);
        return NULL;
    }

    FileHandle* new_fh = calloc(1, sizeof(FileHandle));
    new_fh->sfs = d->sfs;
    new_fh->fcb = ffb;
    new_fh->directory = d->dcb;
    new_fh->current_block = &ffb->header;
    new_fh->pos_in_file = 0;

    return new_fh;
}

int SimpleFS_closeFile(FileHandle* f) {

    if (f->current_block != (BlockHeader*) f->fcb) 
        free(f->current_block);
    free(f->fcb);
    free(f);
    return 0;
}

int SimpleFS_write(FileHandle* f, void* data, int size) {

    int free_space, ret;

    if (f->current_block->block_in_file == 0)
        free_space = max_data_ffb - f->pos_in_file;
    else 
        free_space = max_data_fb - ((f->pos_in_file - max_data_ffb) % max_data_fb);

    if (size <= free_space) {
        if (f->current_block->block_in_file == 0) {
            memcpy(f->fcb->data + f->pos_in_file, data, size);
        } 
        else {
            memcpy(((FileBlock*) f->current_block)->data + (max_data_fb - free_space), data, size);
            ret = DiskDriver_writeBlock(f->sfs->disk, f->current_block, f->current_block->block_in_disk);
            if (ret == -1) {
                if (DEBUG) printf("[SFS - write] Cannot write on disk.\n");
                return -1; 
            }
        }
        f->pos_in_file += size;
        f->fcb->fcb.size_in_bytes += size;

        ret = DiskDriver_writeBlock(f->sfs->disk, f->fcb, f->fcb->header.block_in_disk);
        if (ret == -1) {
            if (DEBUG) printf("[SFS - write] Cannot write on disk.\n");
            return -1; 
        }

        return size;
    }
    else if (f->current_block->next_block != -1) {
        if (f->current_block->block_in_file == 0) {
            memcpy(f->fcb->data + f->pos_in_file, data, free_space);
        } 
        else {
            memcpy(((FileBlock*) f->current_block)->data + (max_data_fb - free_space), data, free_space);
            ret = DiskDriver_writeBlock(f->sfs->disk, (FileBlock*) f->current_block, f->current_block->block_in_disk);
            if (ret == -1) {
                if (DEBUG) printf("[SFS - write] Cannot write on disk.\n");
                return -1; 
            }
        }

        f->pos_in_file += free_space;
        f->fcb->fcb.size_in_bytes += free_space;
        f->fcb->fcb.size_in_blocks += 1;

        FileBlock* next = calloc(1, sizeof(FileBlock));
        ret = DiskDriver_readBlock(f->sfs->disk, next, f->current_block->next_block);
        if (ret == -1) {
            if (DEBUG) printf("[SFS - write] Cannot read from disk.\n");
            return -1; 
        }

        if (f->current_block != (BlockHeader*) f->fcb)
            free(f->current_block);

        f->current_block = &next->header;
        int written = SimpleFS_write(f, data + free_space, size - free_space);
        if (written == -1) 
            return -1;
        return free_space + written;
    }
    else {
        int free_block = DiskDriver_getFreeBlock(f->sfs->disk, 0);
        if (free_block == -1) {
            if (DEBUG) printf("[SFS - write] No free block.\n");
            return -1;
        }

        FileBlock new_block = {0};
        new_block.header.previous_block = f->current_block->block_in_disk;
        new_block.header.next_block = -1;
        new_block.header.block_in_file = f->current_block->block_in_file + 1;
        new_block.header.block_in_disk = free_block;
        
        f->current_block->next_block = free_block;

        ret = DiskDriver_writeBlock(f->sfs->disk, &new_block, free_block);
        if (ret == -1) {
            if (DEBUG) printf("[SFS - write] Cannot write on disk.\n");
            return -1; 
        }

        int written = SimpleFS_write(f, data, size);
        if (written == -1)
            return -1;
        return written;
    }
}

int SimpleFS_read(FileHandle* f, void* data, int size) {

    int readable_bytes, ret;

    if (f->current_block->block_in_file == 0) 
        readable_bytes = max_data_ffb - f->pos_in_file;
    else 
        readable_bytes = max_data_fb - ((f->pos_in_file - max_data_ffb) % max_data_fb);

    if (size <= readable_bytes) {
        if (f->current_block->block_in_file == 0) 
            memcpy(data, f->fcb->data, size);
        else
            memcpy(data, ((FileBlock*) f->current_block)->data, size);

        f->pos_in_file += size;
        return size;
    }
    else {
        if (f->current_block->block_in_file == 0) 
            memcpy(data, f->fcb->data, readable_bytes);
        else
            memcpy(data, ((FileBlock*) f->current_block)->data, readable_bytes);
        f->pos_in_file += readable_bytes;

        if (f->current_block->next_block != -1) {     

            FileBlock* next = calloc(1, sizeof(FileBlock));
            ret = DiskDriver_readBlock(f->sfs->disk, next, f->current_block->next_block);
            if (ret == -1) {
                if (DEBUG) printf("[SFS - read] Cannot read from disk.\n");
                return -1; 
            }

            if (f->current_block != (BlockHeader*) f->fcb)
                free(f->current_block);
            
            f->current_block = &next->header;

            int read = SimpleFS_read(f, data + readable_bytes, size - readable_bytes);
            if (read == -1)
                return -1;
            return readable_bytes + read;
        }
        else {
            return readable_bytes;
        }
    }
}
int SimpleFS_changeDir(DirectoryHandle* d, char* dirname) {
    
    if (strcmp(d->dcb->fcb.name, dirname) == 0)
        return 0;

    if (strcmp(dirname, ".") == 0) 
        return 0;

    if (strcmp(dirname, "..") == 0) {
        if (strcmp(d->dcb->fcb.name, "/") == 0)
            return 0;

        free(d->dcb);
        d->dcb = d->directory;
        d->directory = NULL;
        d->current_block = &d->dcb->header;
        d->pos_in_dir = 0;
        d->pos_in_block = 0;
        
        int parent_block = d->dcb->fcb.directory_block;
        if (parent_block != -1) {
            FirstDirectoryBlock* fdb = calloc(1, sizeof(FirstDirectoryBlock));
            int ret = DiskDriver_readBlock(d->sfs->disk, fdb, parent_block);
            if (ret == -1) {
                if (DEBUG) printf("[SFS - changeDir] Cannot read from disk.\n");
                free(fdb);
                return -1;
            }

            d->directory = fdb;
        }

        return 0;
    }

    if (strcmp(dirname, "/") == 0) {
        free(d->dcb);    
        free(d->directory);   

        d->directory = NULL;

        FirstDirectoryBlock* fdb = calloc(1, sizeof(FirstDirectoryBlock));
        int ret = DiskDriver_readBlock(d->sfs->disk, fdb, 0);
        if (ret == -1) {
            if (DEBUG) printf("[SFS - changeDir] Cannot read from disk.\n");
            free(fdb);
            return -1;
        }

        d->dcb = fdb;
        d->current_block = &fdb->header;
        d->pos_in_dir = 0;
        d->pos_in_block = 0;

        return 0;
    }

    int dir_block;
    if ((dir_block = SimpleFS_exists(d, dirname))) {
        if (d->directory != NULL)
            free(d->directory);
        d->directory = d->dcb;

        FirstDirectoryBlock* fdb = calloc(1, sizeof(FirstDirectoryBlock));
        int ret = DiskDriver_readBlock(d->sfs->disk, fdb, dir_block);
        if (ret == -1) {
            if (DEBUG) printf("[SFS - changeDir] Cannot read from disk.\n");
            free(fdb);
            return -1;
        }

        if (fdb->fcb.is_dir == 0) {
            free(fdb);
            if (DEBUG) printf("[SFS - changeDir] Given dirname is not a directory.\n");
            return -1;
        }

        d->dcb = fdb;
        d->current_block = &fdb->header;
        d->pos_in_dir = 0;
        d->pos_in_block = 0;

        return 0;
    }

    if (DEBUG) printf("[SFS - changeDir] Directory doesn't exists.\n");
    return -1;
}
int SimpleFS_mkDir(DirectoryHandle* d, char* dirname) {

    if (SimpleFS_exists(d, dirname)) {
        if (DEBUG) printf("[SFS - mkDir] Directory already exists.\n");
        return -1;
    }

    int max_entries_fdb = (BLOCK_SIZE - sizeof(BlockHeader) -
                           sizeof(FileControlBlock) - sizeof(int)) / sizeof(int);
    int max_entries_db = (BLOCK_SIZE - sizeof(BlockHeader)) / sizeof(int);

    FirstDirectoryBlock* fdb = d->dcb;

    int ret;
    int free_block = DiskDriver_getFreeBlock(d->sfs->disk, 0);
    if (free_block == -1) {
        if (DEBUG) printf("[SFS - mkDir] No free block.\n");
        return -1;
    }

    FirstDirectoryBlock new_fdb = {0};
    new_fdb.header.previous_block = -1;
    new_fdb.header.next_block = -1;
    new_fdb.header.block_in_file = 0;
    new_fdb.header.block_in_disk = free_block;

    new_fdb.fcb.directory_block = fdb->header.block_in_disk;
    new_fdb.fcb.size_in_bytes = BLOCK_SIZE;
    new_fdb.fcb.size_in_blocks = 1;
    new_fdb.fcb.is_dir = 1;
    new_fdb.fcb.idx_in_directory = fdb->num_entries;
    strncpy(new_fdb.fcb.name, dirname, 128);

    new_fdb.num_entries = 0;

    ret = DiskDriver_writeBlock(d->sfs->disk, &new_fdb, free_block);
    if (ret == -1) {            
        if (DEBUG) printf("[SFS - mkDir] Cannot write on disk.\n");
        return -1;
    }

    if (fdb->num_entries < max_entries_fdb) {
        fdb->file_blocks[fdb->num_entries] = free_block;
        fdb->num_entries += 1;
        
        ret = DiskDriver_writeBlock(d->sfs->disk, fdb, fdb->header.block_in_disk);
        if (ret == -1) {
            if (DEBUG) printf("[SFS - mkDir] Cannot write on disk.\n");
            return -1; 
        }
    }
    else {
        int entries = fdb->num_entries - max_entries_fdb;
        if (entries == 0 || entries % max_entries_db == 0) {
            int block_free_block = DiskDriver_getFreeBlock(d->sfs->disk, 0);
            if (block_free_block == -1) {
                if (DEBUG) printf("[SFS - mkDir] No free block.\n");
                return -1;
            }

            DirectoryBlock new_block = {0};
            new_block.header.next_block = -1;
            new_block.header.block_in_disk = block_free_block;

            if (entries == 0) {
                new_block.header.block_in_file = fdb->header.block_in_file + 1;
                new_block.header.previous_block = fdb->header.block_in_disk;
                new_block.file_blocks[0] = free_block;
                fdb->header.next_block = block_free_block;
            }
            else {
                DirectoryBlock last_block;
                ret = DiskDriver_readBlock(d->sfs->disk, &last_block, fdb->header.next_block);
                if (ret == -1) {
                    if (DEBUG) printf("[SFS - mkDir] Cannot read from disk.\n");
                    return -1;
                }
                while (last_block.header.next_block != -1) {
                    ret = DiskDriver_readBlock(d->sfs->disk, &last_block, last_block.header.next_block);
                    if (ret == -1) {
                        if (DEBUG) printf("[SFS - mkDir] Cannot read from disk.\n");
                        return -1;
                    }
                }

                new_block.header.block_in_file = last_block.header.block_in_file + 1;
                new_block.header.previous_block = last_block.header.block_in_disk;
                new_block.file_blocks[0] = free_block;
                last_block.header.next_block = block_free_block;

                ret = DiskDriver_writeBlock(d->sfs->disk, &last_block, last_block.header.block_in_disk);
                if (ret == -1) {
                    if (DEBUG) printf("[SFS - mkDir] Cannot write on disk.\n");
                    return -1; 
                }
            }
            ret = DiskDriver_writeBlock(d->sfs->disk, &new_block, new_block.header.block_in_disk);
            if (ret == -1) {
                if (DEBUG) printf("[SFS - mkDir] Cannot write on disk.\n");
                return -1; 
            }
        }
        else {
            DirectoryBlock db;
            ret = DiskDriver_readBlock(d->sfs->disk, &db, fdb->header.next_block);
            if (ret == -1) {
                if (DEBUG) printf("[SFS - mkDir] Cannot read from disk.\n");
                return -1;
            }
            while (db.header.next_block != -1) {
                ret = DiskDriver_readBlock(d->sfs->disk, &db, db.header.next_block);
                if (ret == -1) {
                    if (DEBUG) printf("[SFS - mkDir] Cannot read from disk.\n");
                    return -1;
                }
            }

            db.file_blocks[entries % max_entries_db] = free_block;
            ret = DiskDriver_writeBlock(d->sfs->disk, &db, db.header.block_in_disk);
            if (ret == -1) {
                if (DEBUG) printf("[SFS - mkDir] Cannot write on disk.\n");
                return -1; 
            }
        }
        fdb->num_entries += 1;
        fdb->fcb.size_in_bytes += BLOCK_SIZE;
        fdb->fcb.size_in_blocks += 1;

        ret = DiskDriver_writeBlock(d->sfs->disk, fdb, fdb->header.block_in_disk);
        if (ret == -1) {
            if (DEBUG) printf("[SFS - mkDir] Cannot write on disk.\n");
            return -1; 
        }
    }
    return 0;
}

int SimpleFS_remove(DirectoryHandle* d, char* filename) {

    int first_block;
    if ((first_block = SimpleFS_exists(d, filename)) == 0) {
        if (DEBUG) printf("[SFS - remove] File/Dir doesn't exists.\n");
        return -1;
    }

    int ret;

    void* block = calloc(1, BLOCK_SIZE);
    ret = DiskDriver_readBlock(d->sfs->disk, block, first_block);
    if (ret == -1) {
        if (DEBUG) printf("[SFS - remove] Cannot read from disk.\n");
        return -1; 
    }

    if (((FirstFileBlock*) block)->fcb.is_dir == 0) {
        ret = SimpleFS_removeFile(d, (FirstFileBlock*) block);
    }
    else if (((FirstDirectoryBlock*) block)->num_entries == 0) {
        ret = SimpleFS_removeDir(d, (FirstDirectoryBlock*) block);
    }
    else {
        if (DEBUG) printf("[SFS - remove] Directory is not empty.\n");
        ret = -1;
    }

    free(block);
    return ret;
}