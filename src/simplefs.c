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
