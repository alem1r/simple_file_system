#include <bitmap.h>


BitMapEntryKey BitMap_blockToIndex(int num) {
    BitMapEntryKey entry = {
        .entry_num = num >> 3,
        .bit_num = num % 0x7 
    };
    return entry;
}

int BitMap_indexToBlock(int entry, uint8_t bit_num) {
    return (entry << 3) | (bit_num & 0x7);
}

int BitMap_get(BitMap* bmap, int start, int status) {
    int idx = start;
    while (idx < bmap->num_bits) {
        BitMapEntryKey entry = BitMap_blockToIndex(idx);
        int current_status = bmap->entries[entry.entry_num] >> entry.bit_num & 0x1;
        if (current_status == status)
            return idx;
        idx ++;
    }
    return -1;
}

int BitMap_set(BitMap* bmap, int pos, int status) {
    if (pos >= bmap->num_bits)
        return -1;

    BitMapEntryKey entry = BitMap_blockToIndex(pos);
    bmap->entries[entry.entry_num] &= ~(1 << entry.bit_num);
    bmap->entries[entry.entry_num] |= status << entry.bit_num;
        
    return 0;
}
