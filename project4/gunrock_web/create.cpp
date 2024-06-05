#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>

#include "../include/LocalFileSystem.h"
#include "../include/Disk.h"
#include "../include/ufs.h"

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
    if (argc != 5) {
        cout << argv[0] << ": diskImageFile parentInodeNumber type name" << endl;
        return 1;
    }

    Disk *diskImage = new Disk(argv[1], UFS_BLOCK_SIZE); 
    LocalFileSystem localFS(diskImage);

    int parentInodeNumber = stoi(argv[2]);
    int type = stoi(argv[3]);
    string name = argv[4];

    super_t superBlock;
    localFS.readSuperBlock(&superBlock);
    
    int createStatus = localFS.create(parentInodeNumber, type, name);

    if (createStatus < 0) {
        cerr << "Error While creating" << endl;
    }
    else {
        cout << "create successful" << endl;
    }
    if (createStatus == -EINVALIDINODE) {
        cerr << "Invalid Inode" << endl;
    } else if (createStatus == -EDIRNOTEMPTY) {
        cerr << "Directory Not Empty" << endl;
    } else if (createStatus == -EINVALIDSIZE) {
        cerr << "Invalid Size" << endl;
    } else if (createStatus == -EINVALIDTYPE) {
        cerr << "Invalid Type" << endl;
    } else if (createStatus == -ENOTENOUGHSPACE) {
        cerr << "Not Enough Space" << endl;
    } else if (createStatus == -EUNLINKNOTALLOWED) {
        cerr << "Unlinking Not Allowed" << endl;
    } else if (createStatus == -EINVALIDNAME) {
        cerr << "Invalid Name" << endl;
    }
    return 0;
}