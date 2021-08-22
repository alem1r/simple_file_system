#include <disk_driver.h>


#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>     


static void DiskDriver_initDiskHeader(DiskHeader* dh, int num_blocks, int bitmap_size, 
                                        int free_blocks, int first_free_block, 
                                        char* bitmap_data) {
    if (!dh)
        return;

    dh->num_blocks = num_blocks;
    dh->bitmap_blocks = num_blocks;
    dh->bitmap_entries = bitmap_size;
    dh->free_blocks = free_blocks;
    dh->first_free_block = first_free_block;

    bzero(bitmap_data, bitmap_size);
}


void DiskDriver_init(DiskDriver* disk, const char* filename, int num_blocks) {
    int ret;
    int exists = access(filename, F_OK) != -1;
    int fd = open(filename, O_CREAT | O_RDWR, 0600);
    CHECK_ERROR(fd == -1, "[DD - init] open failed.\n");


    int bitmap_size = num_blocks >> 3;
    if (num_blocks & 0x7) bitmap_size ++;

    int zone_size = sizeof(DiskHeader) + bitmap_size + BLOCK_SIZE * num_blocks;
    
    if (!exists) {
        ret = posix_fallocate(fd, 0, zone_size);
        CHECK_ERROR(ret != 0, "[DD - init] fallocate failed.\n"); 
    }
    
    void * zone = mmap(0, zone_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    CHECK_ERROR(!zone, "[DD - init] mmap failed.\n");

    disk->header = (DiskHeader*)zone;
    disk->bitmap_data = (char*)zone + sizeof(DiskHeader);
    disk->fd = fd;

    if (exists) {
        int difference = num_blocks - disk->header->num_blocks;
        if (difference > 0) { 
            ret = posix_fallocate(disk->fd, zone_size - difference * BLOCK_SIZE, zone_size);
            CHECK_ERROR(ret != 0, "[DD - init] fallocate failed.\n"); 
            DiskDriver_initDiskHeader(disk->header, num_blocks, bitmap_size, 
                                        disk->header->free_blocks + difference, 
                                        disk->header->first_free_block,
                                        disk->bitmap_data + disk->header->bitmap_entries);
        }
    } 
    else {
        DiskDriver_initDiskHeader(disk->header, num_blocks, bitmap_size, 
                                    num_blocks, 0, disk->bitmap_data); 
    }
}

int DiskDriver_readBlock(DiskDriver* disk, void* dest, int block_num) {
    if (block_num >= disk->header->num_blocks || block_num < 0)
        return -1;

    BitMap bmap = {
        .num_bits = disk->header->bitmap_blocks,
        .entries = disk->bitmap_data
    };
    if (BitMap_get(&bmap, block_num, 0) == block_num)
        return -1;
    
    void * blocks_start = disk->bitmap_data + disk->header->bitmap_entries;
    memcpy(dest, blocks_start + block_num * BLOCK_SIZE, BLOCK_SIZE);
    return 0;
}

int DiskDriver_writeBlock(DiskDriver* disk, void* src, int block_num) {
    if (block_num >= disk->header->num_blocks || block_num < 0)
        return -1;
    
    if (block_num == disk->header->first_free_block) 
        disk->header->first_free_block = DiskDriver_getFreeBlock(disk, block_num + 1);

    BitMap bmap = {
        .num_bits = disk->header->bitmap_blocks,
        .entries = disk->bitmap_data
    };
    if (BitMap_get(&bmap, block_num, 0) == block_num)
        disk->header->free_blocks --;

    if (BitMap_set(&bmap, block_num, 1) == -1)
        return -1;

    void * blocks_start = disk->bitmap_data + disk->header->bitmap_entries;
    memcpy(blocks_start + block_num * BLOCK_SIZE, src, BLOCK_SIZE);
    return 0;
}


