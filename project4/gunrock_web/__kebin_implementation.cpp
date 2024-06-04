#include <iostream>
#include <string>
#include <vector>
#include <assert.h>

#include "LocalFileSystem.h"
#include "ufs.h"

#include <cstring>
#include <cmath>

using namespace std;

bool getBit(const unsigned char * bitmap, const int bit) {
  int theIdxToCheck = bit / 8;
  int theBitOffset = bit % 8;
  unsigned char byte = bitmap[theIdxToCheck];

  return byte & (1 << theBitOffset);
}

void setBit(unsigned char * bitmap, const int bit) {
  int theIdxToCheck = bit / 8;
  int theBitOffset = bit % 8;
  unsigned char byte = bitmap[theIdxToCheck];

  bitmap[theIdxToCheck] = byte | (1 << theBitOffset);
}

void clearBit(unsigned char * bitmap, const int bit) {
  int theIdxToCheck = bit / 8;
  int theBitOffset = bit % 8;
  unsigned char byte = bitmap[theIdxToCheck];

  bitmap[theIdxToCheck] = byte | ~(1 << theBitOffset);
}

int countNumberOfAvailableBits(const unsigned char * bitmap, const int size) {
  int theNumberOfAvailableBits = 0;
  
  for (int i = 0; i < size; ++i)
    if (!getBit(bitmap, i))
      ++theNumberOfAvailableBits;

  return theNumberOfAvailableBits;
}

int ceilDiv(const int num, const int den) {
  return (num + den - 1) / den;
}

// Note - len is the length of the array, number of bits is len * 8
int getFirstAvailableBit(const unsigned char * bitmap, const int len) {
  for (int i = 0; i < len * 8; ++i) {
    if (getBit(bitmap, i)) {
      return i;
    }
  }

  return -1;
}

inline bool isValidInodeNumber(const super_t & super, const int num) {
  return num >= 0 && num < super.num_inodes;
}

inline bool isValidName(const string name) {
  // Need one more char for null terminating
  return name.size() < DIR_ENT_NAME_SIZE;
}

inline bool unlinkAllowed(const string name) {
  return name != "." && name != "..";
}

int countBlocks(const inode_t & theInode) {
  if (theInode.type == UFS_DIRECTORY) {
    int count = 0;
    for (int i = 0; i < DIRECT_PTRS; ++i) {
      if ((int)theInode.direct[i] != -1)
        ++count;
      else
        break;
    }

    return count;
  }
  else { // type is UFS_REGULAR_FILE
    return ceilDiv(theInode.size, UFS_BLOCK_SIZE);
  }
}


LocalFileSystem::LocalFileSystem(Disk *disk) {
    this->disk = disk;
}

void LocalFileSystem::readSuperBlock(super_t *super) {
    if (disk == nullptr) {
        return;
    }
    char buffer[UFS_BLOCK_SIZE];
    disk->readBlock(0, buffer);
    memcpy(super, buffer, sizeof(super_t));
}

int LocalFileSystem::lookup(int parentInodeNumber, string name) {
    inode_t dirInode;
    if (stat(parentInodeNumber, &dirInode) != 0 || dirInode.type != UFS_DIRECTORY) {
        return -EINVALIDINODE;
    }

    int numDirectoryEntries = dirInode.size / sizeof(dir_ent_t);
    char* buffer = new char[dirInode.size];

    if (read(parentInodeNumber, buffer, dirInode.size) != dirInode.size) {
        delete[] buffer;
        return -EINVALIDINODE;
    }

    dir_ent_t* entries = reinterpret_cast<dir_ent_t*>(buffer);
    int inodeNumber = -1;

    for (int i = 0; i < numDirectoryEntries; i++) {
        if (string(entries[i].name) == name) {
            inodeNumber = entries[i].inum;
            break;
        }
    }

    delete[] buffer;
    return inodeNumber;
}

int LocalFileSystem::stat(int inodeNumber, inode_t *inode) {
    if (disk == nullptr || inode == nullptr) {
        return -1;
    }

    super_t super;
    readSuperBlock(&super); // First, read the superblock

    if (inodeNumber < 0 || inodeNumber >= super.num_inodes) {
        return -EINVALIDINODE; // Invalid inode number
    }

    int inodeBlockNumber = super.inode_region_addr + (inodeNumber * sizeof(inode_t)) / UFS_BLOCK_SIZE;
    int offsetInBlock = (inodeNumber * sizeof(inode_t)) % UFS_BLOCK_SIZE;

    char buffer[UFS_BLOCK_SIZE];
    disk->readBlock(inodeBlockNumber, buffer);

    memcpy(inode, buffer + offsetInBlock, sizeof(inode_t));
    return 0;
}

int LocalFileSystem::read(int inodeNumber, void *buffer, int size) {
    if (disk == nullptr || buffer == nullptr || size < 0 || size > MAX_FILE_SIZE) {
        return -EINVALIDSIZE; // Invalid input
    }

    inode_t inode;
    if (stat(inodeNumber, &inode) != 0) {
        return -EINVALIDINODE; // Error reading inode
    }

    if (inode.size == 0 || size == 0) {
        return 0;
    }

    if (size > inode.size) {
        size = inode.size;
    }

    int bytesToRead = size;
    int bytesRead = 0;
    char* bufPtr = static_cast<char*>(buffer);

    for (int i = 0; i < DIRECT_PTRS && bytesRead < bytesToRead; i++) {
        if (inode.direct[i] == 0) {
            break; 
        }

        char tempBuffer[UFS_BLOCK_SIZE];
        try {
            disk->readBlock(inode.direct[i], tempBuffer);
        } catch (...) {
            return -EINVALIDTYPE; 
        }

        int remainingFileBytes = inode.size - (i * UFS_BLOCK_SIZE);
        int blockLimit = min(UFS_BLOCK_SIZE, remainingFileBytes);
        int toRead = min(blockLimit, bytesToRead - bytesRead);

        memcpy(bufPtr, tempBuffer, toRead);
        bufPtr += toRead;
        bytesRead += toRead;
    }

    return bytesRead;
}

int LocalFileSystem::create(int parentInodeNumber, int type, string name) {
    if (disk == nullptr || name.size() >= DIR_ENT_NAME_SIZE) {
        return -EINVALIDSIZE;
    }

    disk->beginTransaction();

    super_t super;
    readSuperBlock(&super);

    unsigned char inodeBitmap[UFS_BLOCK_SIZE];
    disk->readBlock(super.inode_bitmap_addr, inodeBitmap);

    int newInodeNumber = -1;
    for (int i = 0; i < super.num_inodes; ++i) {
        if ((inodeBitmap[i / 8] & (1 << (i % 8))) == 0) {
            newInodeNumber = i;
            inodeBitmap[i / 8] |= (1 << (i % 8));
            break;
        }
    }

    if (newInodeNumber == -1) {
        disk->rollback();
        return -ENOTENOUGHSPACE;
    }

    inode_t newInode;
    memset(&newInode, 0, sizeof(inode_t));
    newInode.type = type;
    newInode.size = 0;

    int inodeBlockNumber = super.inode_region_addr + (newInodeNumber * sizeof(inode_t)) / UFS_BLOCK_SIZE;
    int offsetInBlock = (newInodeNumber * sizeof(inode_t)) % UFS_BLOCK_SIZE;
    char buffer[UFS_BLOCK_SIZE];
    disk->readBlock(inodeBlockNumber, buffer);
    memcpy(buffer + offsetInBlock, &newInode, sizeof(inode_t));
    disk->writeBlock(inodeBlockNumber, buffer);

    inode_t parentInode;
    if (stat(parentInodeNumber, &parentInode) != 0 || parentInode.type != UFS_DIRECTORY) {
        disk->rollback();
        return (parentInode.type != UFS_DIRECTORY) ? -EINVALIDTYPE : -EINVALIDINODE; // Invalid inode or not a directory
    }

    char* dirBuffer = new char[parentInode.size];
    if (read(parentInodeNumber, dirBuffer, parentInode.size) != parentInode.size) {
        delete[] dirBuffer;
        disk->rollback();
        return -EINVALIDSIZE; // Invalid size read
    }

    dir_ent_t* entries = reinterpret_cast<dir_ent_t*>(dirBuffer);
    bool entryAdded = false;

    for (int i = 0; i < static_cast<int>(parentInode.size / sizeof(dir_ent_t)); ++i) {
        if (entries[i].inum == -1) {
            strncpy(entries[i].name, name.c_str(), DIR_ENT_NAME_SIZE);
            entries[i].inum = newInodeNumber;
            entryAdded = true;
            break;
        }
    }

    if (!entryAdded) {
        delete[] dirBuffer;
        disk->rollback();
        return -ENOTENOUGHSPACE; // No space for new directory entry
    }

    write(parentInodeNumber, dirBuffer, parentInode.size);
    delete[] dirBuffer;

    disk->writeBlock(super.inode_bitmap_addr, inodeBitmap);
    disk->commit();

    return newInodeNumber;
}










int LocalFileSystem::unlink(int parentInodeNumber, std::string name) {
    super_t super;
    readSuperBlock(&super);

    // Meta-level error checks
    if (parentInodeNumber < 0 || parentInodeNumber >= super.num_inodes) {
        return -EINVALIDINODE;
    }

    inode_t parentInode;
    if (stat(parentInodeNumber, &parentInode) != 0 || parentInode.type != UFS_DIRECTORY) {
        return -EINVALIDINODE;
    }

    if (name.empty() || name == "." || name == "..") {
        return -EINVALIDNAME;
    }

    char* dirBuffer = new char[parentInode.size];
    if (read(parentInodeNumber, dirBuffer, parentInode.size) != parentInode.size) {
        delete[] dirBuffer;
        return -EINVALIDINODE; // Cannot read parent inode correctly
    }

    // Parse the directory entries from the buffer
    int numEntries = parentInode.size / sizeof(dir_ent_t);
    dir_ent_t* entries = reinterpret_cast<dir_ent_t*>(dirBuffer);

    // Find the directory entry with the given name
    int foundIndex = -1;
    for (int i = 0; i < numEntries; ++i) {
        if (std::string(entries[i].name) == name) {  // Using std::string comparison
            foundIndex = i;
            break;
        }
    }

    // If the name was not found, there's nothing to do
    if (foundIndex == -1) {
        delete[] dirBuffer;
        return 0; // Name not found is not considered a failure
    }

    // Get the inode of the file/directory to unlink
    inode_t targetInode;
    if (stat(entries[foundIndex].inum, &targetInode) != 0) {
        delete[] dirBuffer;
        return -EINVALIDINODE; // Cannot read target inode correctly
    }

    // Check if the target is a non-empty directory
    if (targetInode.type == UFS_DIRECTORY) {
        if (targetInode.size > static_cast<int>(2 * sizeof(dir_ent_t))) {
            delete[] dirBuffer;
            return -EDIRNOTEMPTY; // Directory is not empty
        }
    }

    // Begin the transaction
    disk->beginTransaction();

    try {
        // Deallocate the target inode and its data blocks
        unsigned char inodeBitmap[UFS_BLOCK_SIZE];
        disk->readBlock(super.inode_bitmap_addr, inodeBitmap);
        clearBit(inodeBitmap, entries[foundIndex].inum);

        unsigned char dataBitmap[UFS_BLOCK_SIZE];
        disk->readBlock(super.data_bitmap_addr, dataBitmap);

        for (int i = 0; i < DIRECT_PTRS; ++i) {
            if (targetInode.direct[i] != 0) {
                clearBit(dataBitmap, targetInode.direct[i]);
            }
        }

        // Remove the directory entry
        entries[foundIndex].inum = -1;
        memset(entries[foundIndex].name, 0, DIR_ENT_NAME_SIZE);

        // Compact the directory entries to fill the gap
        for (int i = foundIndex; i < numEntries - 1; ++i) {
            entries[i] = entries[i + 1];
        }
        // Clear the last entry
        entries[numEntries - 1].inum = -1;
        memset(entries[numEntries - 1].name, 0, DIR_ENT_NAME_SIZE);

        // Update the parent inode size
        parentInode.size -= sizeof(dir_ent_t);

        // Write back the parent directory's data
        write(parentInodeNumber, dirBuffer, parentInode.size);

        // Free the directory buffer after use
        delete[] dirBuffer;

        // Update the bitmaps on disk
        disk->writeBlock(super.inode_bitmap_addr, inodeBitmap);
        disk->writeBlock(super.data_bitmap_addr, dataBitmap);

        // Write back the updated parent inode
        int inodeBlockNumber = super.inode_region_addr + (parentInodeNumber * sizeof(inode_t)) / UFS_BLOCK_SIZE;
        int offsetInBlock = (parentInodeNumber * sizeof(inode_t)) % UFS_BLOCK_SIZE;

        char inodeBuffer[UFS_BLOCK_SIZE];
        disk->readBlock(inodeBlockNumber, inodeBuffer);
        memcpy(inodeBuffer + offsetInBlock, &parentInode, sizeof(inode_t));
        disk->writeBlock(inodeBlockNumber, inodeBuffer);

        // Commit the transaction
        disk->commit();
    } catch (...) {
        // Rollback in case of any errors
        disk->rollback();
        delete[] dirBuffer;
        return -ENOTENOUGHSPACE; // Using this error code for any transaction issues
    }

    return 0;
}















int LocalFileSystem::write(int inodeNumber, const void *buffer, int size) {
    super_t super_block;
    readSuperBlock(&super_block);

    if (!isValidInodeNumber(super_block, inodeNumber))
        return -EINVALIDINODE;

    if (size < 0 || size > MAX_FILE_SIZE)
        return -EINVALIDSIZE;

    inode_t theInode;
    this->stat(inodeNumber, &theInode);

    if (theInode.type != UFS_REGULAR_FILE)
        return -EINVALIDTYPE;

    const int theLenOfDataBitmapArr = super_block.data_bitmap_len * UFS_BLOCK_SIZE;
    unsigned char theDataBitmap[theLenOfDataBitmapArr];
    this->readDataBitmap(&super_block, theDataBitmap);

    int theNumberOfBlocksPresent = ceilDiv(theInode.size, UFS_BLOCK_SIZE);
    int theNumberOfBlocksNeeded = ceilDiv(size, UFS_BLOCK_SIZE);

    int theNumberOfBlocksToAllocate = max(0, theNumberOfBlocksNeeded - theNumberOfBlocksPresent);
    int theNumberOfBlocksToDeallocate = max(0, theNumberOfBlocksPresent - theNumberOfBlocksNeeded);

    if (theNumberOfBlocksNeeded >= DIRECT_PTRS)
        return -ENOTENOUGHSPACE;

    int freeBlocksCount = 0;
    for (int i = 0; i < theLenOfDataBitmapArr * 8; ++i) {
        if (!getBit(theDataBitmap, i))
            freeBlocksCount++;
    }
    if (freeBlocksCount < theNumberOfBlocksToAllocate)
        return -ENOTENOUGHSPACE;

    for (int i = 0; i < theNumberOfBlocksToAllocate; ++i) {
        int idx = theNumberOfBlocksPresent + i;
        int theAvailableBlockNumber = getFirstAvailableBit(theDataBitmap, theLenOfDataBitmapArr);
        if (theAvailableBlockNumber == -1)
            return -ENOTENOUGHSPACE;
        setBit(theDataBitmap, theAvailableBlockNumber);

        theInode.direct[idx] = theAvailableBlockNumber;
    }

    for (int i = 1; i <= theNumberOfBlocksToDeallocate; ++i) {
        int idx = theNumberOfBlocksPresent - i;
        int theBlockNumberToFree = theInode.direct[idx];

        clearBit(theDataBitmap, theBlockNumberToFree);
    }

    int theSizeOnTheLastBlock = size % UFS_BLOCK_SIZE;
    if (theSizeOnTheLastBlock == 0)
        theSizeOnTheLastBlock = UFS_BLOCK_SIZE;

    this->disk->beginTransaction();
    this->writeDataBitmap(&super_block, theDataBitmap);

    for (int i = 0; i < theNumberOfBlocksNeeded; ++i) {
        int theBufferOffset = i * UFS_BLOCK_SIZE;
        int theBlockNumber = theInode.direct[i];
        this->disk->writeBlock(theBlockNumber, const_cast<void*>(static_cast<const void*>(static_cast<const char*>(buffer) + theBufferOffset)));
    }

    theInode.size = size;

    inode_t *inodes = new inode_t[super_block.num_inodes];
    this->readInodeRegion(&super_block, inodes);

    inodes[inodeNumber] = theInode;

    this->writeInodeRegion(&super_block, inodes);

    delete[] inodes;

    this->disk->commit();

    return size;
}

void LocalFileSystem::readInodeBitmap(super_t *super, unsigned char *inodeBitmap) {
    int inodeBitmapStartBlock = super->inode_bitmap_addr;
    int inodeBitmapLen = super->inode_bitmap_len * UFS_BLOCK_SIZE;
    for (int i = 0; i < inodeBitmapLen / UFS_BLOCK_SIZE; ++i) {
        this->disk->readBlock(inodeBitmapStartBlock + i, inodeBitmap + i * UFS_BLOCK_SIZE);
    }
}

void LocalFileSystem::writeInodeBitmap(super_t *super, unsigned char *inodeBitmap) {
    int inodeBitmapStartBlock = super->inode_bitmap_addr;
    int inodeBitmapLen = super->inode_bitmap_len * UFS_BLOCK_SIZE;
    for (int i = 0; i < inodeBitmapLen / UFS_BLOCK_SIZE; ++i) {
        this->disk->writeBlock(inodeBitmapStartBlock + i, inodeBitmap + i * UFS_BLOCK_SIZE);
    }
}

void LocalFileSystem::readDataBitmap(super_t *super, unsigned char *dataBitmap) {
    int dataBitmapStartBlock = super->data_bitmap_addr;
    int dataBitmapLen = super->data_bitmap_len * UFS_BLOCK_SIZE;
    for (int i = 0; i < dataBitmapLen / UFS_BLOCK_SIZE; ++i) {
        this->disk->readBlock(dataBitmapStartBlock + i, dataBitmap + i * UFS_BLOCK_SIZE);
    }
}

void LocalFileSystem::writeDataBitmap(super_t *super, unsigned char *dataBitmap) {
    int dataBitmapStartBlock = super->data_bitmap_addr;
    int dataBitmapLen = super->data_bitmap_len * UFS_BLOCK_SIZE;
    for (int i = 0; i < dataBitmapLen / UFS_BLOCK_SIZE; ++i) {
        this->disk->writeBlock(dataBitmapStartBlock + i, dataBitmap + i * UFS_BLOCK_SIZE);
    }
}

void LocalFileSystem::readInodeRegion(super_t *super, inode_t *inodes) {
    int inodeRegionStartBlock = super->inode_region_addr;
    int inodeRegionLen = super->inode_region_len * UFS_BLOCK_SIZE;
    for (int i = 0; i < inodeRegionLen / UFS_BLOCK_SIZE; ++i) {
        this->disk->readBlock(inodeRegionStartBlock + i, reinterpret_cast<unsigned char*>(inodes) + i * UFS_BLOCK_SIZE);
    }
}

void LocalFileSystem::writeInodeRegion(super_t *super, inode_t *inodes) {
    int inodeRegionStartBlock = super->inode_region_addr;
    int inodeRegionLen = super->inode_region_len * UFS_BLOCK_SIZE;
    for (int i = 0; i < inodeRegionLen / UFS_BLOCK_SIZE; ++i) {
        this->disk->writeBlock(inodeRegionStartBlock + i, reinterpret_cast<unsigned char*>(inodes) + i * UFS_BLOCK_SIZE);
    }
}