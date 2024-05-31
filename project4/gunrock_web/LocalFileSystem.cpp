#include <iostream>
#include <string>
#include <vector>
#include <assert.h>

#include "LocalFileSystem.h"
#include "ufs.h"

#include <cstring>
#include <cmath>

using namespace std;

// helper constants
const int THE_INODES_PER_BLOCK_CONSTANT = UFS_BLOCK_SIZE / sizeof(inode_t);
const int THE_ENTRIES_PER_BLOCK_CONSTANT = UFS_BLOCK_SIZE / sizeof(dir_ent_t);

LocalFileSystem::LocalFileSystem(Disk *disk) {
  this->disk = disk;
}

void LocalFileSystem::readSuperBlock(super_t *super) {
  char buffer[UFS_BLOCK_SIZE];
  this->disk->readBlock(0, buffer);
  memcpy(super, buffer, sizeof(super_t));
}

int LocalFileSystem::lookup(int parentInodeNumber, string name) {
  super_t super_block;
  readSuperBlock(&super_block);

  if (!isValidInodeNumber(super_block, parentInodeNumber))
    return -EINVALIDINODE;

  inode_t theParentInode;
  this->stat(parentInodeNumber, &theParentInode);

  if (theParentInode.type != UFS_DIRECTORY)
    return -EINVALIDINODE;

  // TODO: implement ??

  return 0;
}

int LocalFileSystem::stat(int inodeNumber, inode_t *inode) {
  super_t super_block;
  readSuperBlock(&super_block);

  if (!isValidInodeNumber(super_block, inodeNumber))
    return -EINVALIDINODE;

  int theBlockToRead = super_block.inode_region_addr + inodeNumber * sizeof(inode_t) / UFS_BLOCK_SIZE;
  int theInodeOffset = inodeNumber * sizeof(inode_t) % UFS_BLOCK_SIZE;

  char buffer[UFS_BLOCK_SIZE];
  this->disk->readBlock(theBlockToRead, buffer);

  memcpy(inode, buffer + theInodeOffset, sizeof(inode_t));

  return 0;
}

int LocalFileSystem::read(int inodeNumber, void *buffer, int size) {
  super_t super_block;
  readSuperBlock(&super_block);

  if (!isValidInodeNumber(super_block, inodeNumber))
    return -EINVALIDINODE;

  if (size < 0)
    return -EINVALIDSIZE;

  // Stat the inode
  inode_t theInode;
  this->stat(inodeNumber, &theInode);

  int theActualSize = min(size, theInode.size);
  int theNumberOfBlocksToRead = ceilDiv(theActualSize, UFS_BLOCK_SIZE);
  int theSizeOnTheLastBlock = theActualSize % UFS_BLOCK_SIZE;
  if (theSizeOnTheLastBlock == 0) {
    theSizeOnTheLastBlock = UFS_BLOCK_SIZE;
  }

  // Read everything the inode points to
  for (int i = 0; i < theNumberOfBlocksToRead; ++i) {
    int theBlockToRead = theInode.direct[i];
  
    unsigned char theTempBuffer[UFS_BLOCK_SIZE];

    disk->readBlock(theBlockToRead, theTempBuffer);

    int theOffsetInTheBuffer = i * UFS_BLOCK_SIZE;
    bool isLastBlock = i == theNumberOfBlocksToRead - 1;
    int theAmountToCopy = isLastBlock ? theSizeOnTheLastBlock : UFS_BLOCK_SIZE;
    memcpy(buffer + theOffsetInTheBuffer, theTempBuffer, theAmountToCopy);
  }

  return 0;
}

int LocalFileSystem::create(int parentInodeNumber, int type, string name) {
  super_t super_block;
  readSuperBlock(&super_block);

  // Check input
  #pragma region
  // Check if it's a valid inode and valid name
  if (!isValidInodeNumber(super_block, parentInodeNumber))
    return -EINVALIDINODE;

  if (!isValidName(name))
    return -EINVALIDNAME;

  // Make sure it's a valid type
  if (type != UFS_DIRECTORY && type != UFS_REGULAR_FILE)
    return -EINVALIDTYPE;
  #pragma endregion

  // Get inode bitmap
  const int theLenOfInodeBitmapArr = super_block.data_bitmap_len * UFS_BLOCK_SIZE;
  unsigned char theInodeBitmap[theLenOfInodeBitmapArr];
  this->readInodeBitmap(&super_block, theInodeBitmap);

  // Get inode array
  const int theTotalNumberOfInodes = super_block.inode_region_len * UFS_BLOCK_SIZE / sizeof(inode_t);
  inode_t theInodes[theTotalNumberOfInodes];
  readInodeRegion(&super_block, theInodes);

  // Get data bitmap
  const int theLenOfDataBitmapArr = super_block.data_bitmap_len * UFS_BLOCK_SIZE;
  unsigned char theDataBitmap[theLenOfDataBitmapArr];
  this->readDataBitmap(&super_block, theDataBitmap);

  // Get parent inode
  inode_t parentInode = theInodes[parentInodeNumber];
  #pragma region
  // Make sure it's a directory
  if (parentInode.type != UFS_DIRECTORY)
    return -EINVALIDINODE;

  // Make sure we have enough space in the inode to add an entry
  if (parentInode.size == UFS_BLOCK_SIZE * DIRECT_PTRS)
    return -ENOTENOUGHSPACE;
  #pragma endregion

  // Check if the name already exists
  const int theNumberOfParentEntries = parentInode.size / sizeof(dir_ent_t);
  dir_ent_t theParentEntries[theNumberOfParentEntries];
  #pragma region
  this->read(parentInodeNumber, theParentEntries, theNumberOfParentEntries * sizeof(dir_ent_t));
  for (int i = 0; i < theNumberOfParentEntries; ++i) {
    dir_ent_t theEntryToCheck = theParentEntries[i];
    string theEntryName(theEntryToCheck.name);

    if (theEntryName == name) {
      // Check to see if the type matches
      inode_t theInodeToCheck = theInodes[theEntryToCheck.inum];

      if (theInodeToCheck.type == type) {
        return 0;
      }
      else {
        // This is an error if the types don't match
        return -EINVALIDTYPE;
      }
    }
  }
  #pragma endregion

  // Find the first inode bit that is available, and mark it in bitmap
  int theAvailableInodeNumber = getFirstAvailableBit(theInodeBitmap, theLenOfInodeBitmapArr);
  #pragma region
  if (theAvailableInodeNumber < 0)
    return -ENOTENOUGHSPACE;

  setBit(theInodeBitmap, theAvailableInodeNumber);
  #pragma endregion

  // Now create a new inode for the contents of this entry
  #pragma region
  theInodes[theAvailableInodeNumber].type = type;

  if (type == UFS_DIRECTORY) {
    // This is a directory
    // We need to create two entries for it
    // One for "." pointing to this path
    // The other one ".." pointing to parent
    // TODO ??
    // Need to create another new inode

    theInodes[theAvailableInodeNumber].size = 2 * sizeof(dir_ent_t);
  }
  else {
    // This is a file, it's empty
    theInodes[theAvailableInodeNumber].size = 0;
  }
  #pragma endregion

  // Now we need to add an entry
  int theEntriesBlockNumber = -1;
  dir_ent_t theEntriesBlock[THE_ENTRIES_PER_BLOCK_CONSTANT];
  #pragma region
  // Check if we need to create a new block
  if (parentInode.size % UFS_BLOCK_SIZE == 0) {
    // Need to create a new data block
    theEntriesBlock[0].inum = theAvailableInodeNumber;
    strcpy(theEntriesBlock[0].name, name.c_str());

    for (int i = 1; i < THE_ENTRIES_PER_BLOCK_CONSTANT; ++i) {
      theEntriesBlock[i].inum = -1;
    }

    // Read bitmap to find the first available data block to write to
    theEntriesBlockNumber = getFirstAvailableBit(theDataBitmap, theLenOfDataBitmapArr);
    if (theEntriesBlockNumber == -1) {
      return -ENOTENOUGHSPACE;
    }

    // Add the new block to our parent
    int theIdxToAppendToParentInodeDirect = parentInode.size / UFS_BLOCK_SIZE;
    parentInode.direct[theIdxToAppendToParentInodeDirect] = theEntriesBlockNumber;
  }
  else {
    int theDirectSize = parentInode.size / sizeof(dir_ent_t);
    int theLastBlockIdx = theDirectSize - 1;
    theEntriesBlockNumber = parentInode.direct[theLastBlockIdx];
    
    // Read the last block
    this->disk->readBlock(theEntriesBlockNumber, theEntriesBlock);

    // Now add it to the end of all the previous entries
    int theIdxToAddToInEntriesBlock = parentInode.size % UFS_BLOCK_SIZE;
    theEntriesBlock[theIdxToAddToInEntriesBlock].inum = theAvailableInodeNumber;
    strcpy(theEntriesBlock[theIdxToAddToInEntriesBlock].name, name.c_str());
  }

  // Update the size of the parentInode
  parentInode.size += sizeof(dir_ent_t);
  #pragma endregion

  // Now we need to write the changes to the disk (inode bitmap, data bitmap, inode region, any data blocks)
  #pragma region
  this->disk->beginTransaction();
  
  this->writeInodeBitmap(&super_block, theInodeBitmap);
  this->writeDataBitmap(&super_block, theDataBitmap);
  this->writeInodeRegion(&super_block, theInodes);

  this->disk->writeBlock(theEntriesBlockNumber, theEntriesBlock);
  
  this->disk->commit();
  #pragma endregion

  return 0;
}

int LocalFileSystem::write(int inodeNumber, const void *buffer, int size) {
  super_t super_block;
  readSuperBlock(&super_block);

  if (inodeNumber < 0 || inodeNumber >= super_block.num_inodes)
    return -EINVALIDINODE;

  // TODO: ??

  return 0;
}

int LocalFileSystem::unlink(int parentInodeNumber, string name) {
  super_t super_block;
  readSuperBlock(&super_block);

  if (!isValidInodeNumber(super_block, parentInodeNumber))
    return -EINVALIDINODE;

  if (!isValidName(name))
    return -EINVALIDNAME;

  // Get parent inode
  inode_t theParentInode;
  this->stat(parentInodeNumber, &theParentInode);

  // Read blocks in parent inode
  int theNumberOfEntries = theParentInode.size / sizeof(dir_ent_t);
  dir_ent_t theEntries[theNumberOfEntries];
  this->read(parentInodeNumber, theEntries, theNumberOfEntries * sizeof(dir_ent_t));

  for (int i = 0; i < theNumberOfEntries; ++i) {
    if (string(theEntries[i].name) == name) {
      // Found it
      inode_t theEntryInode;
      this->stat(theEntries[i].inum, &theEntryInode);

      if (theEntryInode.type == UFS_DIRECTORY) {
        // This is a directory, only delete if it's empty
      }
      else {
        // This is a file, we can just delete it
      }
    }
  }

  return 0;
}

bool LocalFileSystem::diskHasSpace(super_t *super, int numInodesNeeded, int numDataBytesNeeded, int numDataBlocksNeeded=0) {
  // Check if we can allocate space for the inode
  int theInodeBitmapLength = super->inode_bitmap_len * UFS_BLOCK_SIZE;
  unsigned char theInodeBitmap[theInodeBitmapLength];
  this->readInodeBitmap(super, theInodeBitmap);

  int numberOfAvailableInodes = countNumberOfAvailableBits(theInodeBitmap, theInodeBitmapLength);

  if (numberOfAvailableInodes < numInodesNeeded) {
    return false;
  }

  // Check if we can allocate space for the data
  int theDataBitmapLength = super->data_bitmap_len * UFS_BLOCK_SIZE;
  unsigned char theDataBitmap[theDataBitmapLength];
  this->readDataBitmap(super, theDataBitmap);

  int numberOfAvailableDataBlocks = countNumberOfAvailableBits(theDataBitmap, theDataBitmapLength);

  return numberOfAvailableDataBlocks >= numDataBlocksNeeded + ceilDiv(numDataBytesNeeded, UFS_BLOCK_SIZE);
}

void LocalFileSystem::readInodeBitmap(super_t *super, unsigned char *inodeBitmap) {    
  for (int i = 0; i < super->inode_bitmap_len; ++i) {
    int theBlockToRead = super->inode_bitmap_addr + i;
    auto theBufferStart = inodeBitmap + i * UFS_BLOCK_SIZE;
    disk->readBlock(theBlockToRead, theBufferStart);
  }
}

void LocalFileSystem::writeInodeBitmap(super_t *super, unsigned char *inodeBitmap) {  
  for (int i = 0; i < super->inode_bitmap_len; ++i) {
    int theBlockToRead = super->inode_bitmap_addr + i;
    auto theBufferStart = inodeBitmap + i * UFS_BLOCK_SIZE;
    disk->writeBlock(theBlockToRead, theBufferStart);
  }
}

void LocalFileSystem::readDataBitmap(super_t *super, unsigned char *dataBitmap) {
  for (int i = 0; i < super->inode_bitmap_len; ++i) {
    int theBlockToRead = super->data_bitmap_addr + i;
    auto theBufferStart = dataBitmap + i * UFS_BLOCK_SIZE;
    disk->readBlock(theBlockToRead, theBufferStart);
  }
}

void LocalFileSystem::writeDataBitmap(super_t *super, unsigned char *dataBitmap) {
  for (int i = 0; i < super->inode_bitmap_len; ++i) {
    int theBlockToRead = super->data_bitmap_addr + i;
    auto theBufferStart = dataBitmap + i * UFS_BLOCK_SIZE;
    disk->writeBlock(theBlockToRead, theBufferStart);
  }
}

void LocalFileSystem::readInodeRegion(super_t *super, inode_t *inodes) {
  for (int i = 0; i < super->inode_region_len; ++i) {
    int theBlockToRead = super->inode_region_addr + i;
    auto theBufferStart = inodes + i * UFS_BLOCK_SIZE;
    disk->readBlock(theBlockToRead, theBufferStart);
  }
}

void LocalFileSystem::writeInodeRegion(super_t *super, inode_t *inodes) {
  for (int i = 0; i < super->inode_region_len; ++i) {
    int theBlockToRead = super->inode_region_addr + i;
    auto theBufferStart = inodes + i * UFS_BLOCK_SIZE;
    disk->writeBlock(theBlockToRead, theBufferStart);
  }
}

int countNumberOfAvailableBits(unsigned char * bitmap, int size) {
  int theNumberOfAvailableBits = 0;
  
  for (int i = 0; i < size; ++i)
    if (!getBit(bitmap, i))
      ++theNumberOfAvailableBits;

  return theNumberOfAvailableBits;
}

bool getBit(unsigned char * bitmap, int bit) {
  int theIdxToCheck = bit / 8;
  int theBitOffset = bit % 8;
  unsigned char byte = bitmap[theIdxToCheck];

  return byte & (1 << theBitOffset);
}

void setBit(unsigned char * bitmap, int bit) {
  int theIdxToCheck = bit / 8;
  int theBitOffset = bit % 8;
  unsigned char byte = bitmap[theIdxToCheck];

  bitmap[theIdxToCheck] = byte | (1 << theBitOffset);
}

void clearBit(unsigned char * bitmap, int bit) {
  int theIdxToCheck = bit / 8;
  int theBitOffset = bit % 8;
  unsigned char byte = bitmap[theIdxToCheck];

  bitmap[theIdxToCheck] = byte | ~(1 << theBitOffset);
}

int ceilDiv(int num, int den) {
  return (num + den - 1) / den;
}

// Note - len is the length of the array, number of bits is len * 8
int getFirstAvailableBit(unsigned char * bitmap, int len) {
  for (int i = 0; i < len * 8; ++i) {
    if (getBit(bitmap, i)) {
      return i;
    }
  }

  return -1;
}

inline bool isValidInodeNumber(super_t & super, int num) {
  return num < 0 || num >= super.num_inodes;
}

inline bool isValidName(string & name) {
  // Need one more char for null terminating
  return name.size() < DIR_ENT_NAME_SIZE && name != ".." && name != ".";
}