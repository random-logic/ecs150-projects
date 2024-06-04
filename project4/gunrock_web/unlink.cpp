#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

void printSuperBlock(super_t *superBlock) {       
    cout << "inode_bitmap_addr: " << superBlock->inode_bitmap_addr << endl;
    cout << "inode_bitmap_len:  " << superBlock->inode_bitmap_len << endl;
    cout << "data_bitmap_addr:  " << superBlock->data_bitmap_addr << endl;
    cout << "data_bitmap_len:   " << superBlock->data_bitmap_len << endl;
    cout << "inode_region_addr: " << superBlock->inode_region_addr << endl;
    cout << "inode_region_len:  " << superBlock->inode_region_len << endl;
    cout << "data_region_addr:  " << superBlock->data_region_addr << endl;
    cout << "data_region_len:   " << superBlock->data_region_len << endl;
    cout << "num_inodes:        " << superBlock->num_inodes << endl;
    cout << "num_data:          " << superBlock->num_data << endl;
}

void readFileBlocks(inode_t *inode) {
    int index = 0;
    int inodeBlockSize = (inode->size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;

    cout << "File blocks" << endl;
    while (index < inodeBlockSize) {
        cout << (unsigned int)inode->direct[index] << endl;
        index++;
    }
    cout << endl;
}


int main(int argc, char *argv[]) {
    if (argc != 4) {
        cout << argv[0] << ": diskImageFile parentInodeNumber name" << endl;
        return 1;
    }

    Disk *diskImage = new Disk(argv[1], UFS_BLOCK_SIZE); 
    LocalFileSystem localFS(diskImage);

    int parentInodeNumber = stoi(argv[2]);
    string name = argv[3];

    super_t superBlock;
    localFS.readSuperBlock(&superBlock);
    
    int unlinkStatus = localFS.unlink(parentInodeNumber, name);

    if (unlinkStatus < 0) {
        cerr << "Error While unlinking" << endl;
    }
    if (unlinkStatus == -EINVALIDINODE) {
        cerr << "Invalid Inode" << endl;
    } else if (unlinkStatus == -EDIRNOTEMPTY) {
        cerr << "Directory Not Empty" << endl;
    } else if (unlinkStatus == -EINVALIDSIZE) {
        cerr << "Invalid Size" << endl;
    } else if (unlinkStatus == -EINVALIDTYPE) {
        cerr << "Invalid Type" << endl;
    } else if (unlinkStatus == -ENOTENOUGHSPACE) {
        cerr << "Not Enough Space" << endl;
    } else if (unlinkStatus == -EUNLINKNOTALLOWED) {
        cerr << "Unlinking Not Allowed" << endl;
    } else if (unlinkStatus == -EINVALIDNAME) {
        cerr << "Not name found" << endl;
    }

    return 0;
}