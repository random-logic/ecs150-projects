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

/* #region Helper functions */
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

  bitmap[theIdxToCheck] = byte & ~(1 << theBitOffset);
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
// Gets the first bit that is 0
int getFirstAvailableBit(const unsigned char * bitmap, const int len) {
  for (int i = 0; i < len * 8; ++i) {
    if (!getBit(bitmap, i)) {
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

inline int dataBlockNumToBit(const super_t & super, const int num) {
  return num - super.data_region_addr;
}

inline int dataBitToBlockNum(const super_t & super, const int bit) {
  return bit + super.data_bitmap_addr;
}
/* #endregion Helper functions */

/* #region Class functions */
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

  // Get all the entries  
  for (int i = 0; i < DIRECT_PTRS; ++i) {
    dir_ent_t theEntries[THE_ENTRIES_PER_BLOCK_CONSTANT];
    int theBlockNumberToRead = theParentInode.direct[i];
    if (theBlockNumberToRead == -1)
      return -ENOTFOUND;

    this->disk->readBlock(theParentInode.direct[i], theEntries);

    for (dir_ent_t entry : theEntries) {
      if (entry.inum == -1)
        return -ENOTFOUND;
      if (string(entry.name) == name)
        return entry.inum;
    }
  }

  return -ENOTFOUND;
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

  // Check input
  /* #region */
  if (!isValidInodeNumber(super_block, inodeNumber))
    return -EINVALIDINODE;

  if (size < 0 || size > MAX_FILE_SIZE)
    return -EINVALIDSIZE;
  /* #endregion */

  // Stat the inode
  inode_t theInode;
  /* #region */
  this->stat(inodeNumber, &theInode);

  if (theInode.type != UFS_REGULAR_FILE && theInode.type != UFS_DIRECTORY)
    return -EINVALIDTYPE;
  /* #endregion */

  // Do actual read
  const int theActualSize = min(size, theInode.size);
  const int theNumberOfBlocksToRead = ceilDiv(theActualSize, UFS_BLOCK_SIZE);
  int theSizeOnTheLastBlock = theActualSize % UFS_BLOCK_SIZE;
  if (theSizeOnTheLastBlock == 0)
    theSizeOnTheLastBlock = UFS_BLOCK_SIZE;

  for (int i = 0; i < theNumberOfBlocksToRead; ++i) {
    int theBlockToRead = theInode.direct[i];
    unsigned char theTempBuffer[UFS_BLOCK_SIZE];
    disk->readBlock(theBlockToRead, theTempBuffer);

    int theOffsetInTheBuffer = i * UFS_BLOCK_SIZE;
    bool isLastBlock = i == theNumberOfBlocksToRead - 1;
    int theAmountToCopy = isLastBlock ? theSizeOnTheLastBlock : UFS_BLOCK_SIZE;
    memcpy((unsigned char*)buffer + theOffsetInTheBuffer, theTempBuffer, theAmountToCopy);
  }

  return theActualSize;
}

int LocalFileSystem::create(int parentInodeNumber, int type, string name) {
  super_t super_block;
  readSuperBlock(&super_block);

  // Check input
  /* #region */
  // Check if it's a valid inode and valid name
  if (!isValidInodeNumber(super_block, parentInodeNumber))
    return -EINVALIDINODE;

  if (!isValidName(name))
    return -EINVALIDNAME;

  // Make sure it's a valid type
  if (type != UFS_DIRECTORY && type != UFS_REGULAR_FILE)
    return -EINVALIDTYPE;
  /* #endregion */

  // Get inode bitmap
  const int theLenOfInodeBitmapArr = super_block.data_bitmap_len * UFS_BLOCK_SIZE;
  unsigned char theInodeBitmap[theLenOfInodeBitmapArr];
  this->readInodeBitmap(&super_block, theInodeBitmap);

  // Get inode array
  const int theTotalNumberOfInodes = super_block.num_inodes;
  inode_t theInodes[theTotalNumberOfInodes];
  readInodeRegion(&super_block, theInodes);

  // Get data bitmap
  const int theLenOfDataBitmapArr = super_block.data_bitmap_len * UFS_BLOCK_SIZE;
  unsigned char theDataBitmap[theLenOfDataBitmapArr];
  this->readDataBitmap(&super_block, theDataBitmap);

  // Get parent inode
  inode_t parentInode = theInodes[parentInodeNumber];
  /* #region */
  // Make sure it's a directory
  if (parentInode.type != UFS_DIRECTORY)
    return -EINVALIDINODE;

  // Make sure we have enough space in the inode to add an entry
  if (parentInode.size == UFS_BLOCK_SIZE * DIRECT_PTRS)
    return -ENOTENOUGHSPACE;
  /* #endregion */

  // Check if the name already exists
  const int theNumberOfParentEntries = parentInode.size / sizeof(dir_ent_t);
  dir_ent_t theParentEntries[theNumberOfParentEntries];
  /* #region */
  read(parentInodeNumber, theParentEntries, theNumberOfParentEntries * sizeof(dir_ent_t));
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
  /* #endregion */

  // Find the first inode bit that is available, and mark it in bitmap
  const int theAvailableInodeNumber = getFirstAvailableBit(theInodeBitmap, theLenOfInodeBitmapArr);
  /* #region */
  if (theAvailableInodeNumber < 0 || theAvailableInodeNumber >= super_block.num_inodes)
    return -ENOTENOUGHSPACE;

  setBit(theInodeBitmap, theAvailableInodeNumber);
  /* #endregion */

  // Now create a new inode for the contents of this entry
  int availableCreatedDataBlockNumber = -1;
  dir_ent_t theCreatedBlock[THE_ENTRIES_PER_BLOCK_CONSTANT];
  /* #region */
  theInodes[theAvailableInodeNumber].type = type;

  if (type == UFS_DIRECTORY) {
    // This is a directory
    // We need to create two entries for it
    // One for "." pointing to this path
    // The other one ".." pointing to parent

    // Get the first available data block, and set it in bitmap
    const int availableCreatedDataBit = getFirstAvailableBit(theDataBitmap, theLenOfDataBitmapArr);

    if (availableCreatedDataBit < 0 || availableCreatedDataBit >= super_block.num_data)
      return -ENOTENOUGHSPACE;

    setBit(theDataBitmap, availableCreatedDataBit);
    availableCreatedDataBlockNumber = dataBitToBlockNum(super_block, availableCreatedDataBit);
    
    // Init the entries block appropriately
    theCreatedBlock[0].inum = theAvailableInodeNumber;
    strcpy(theCreatedBlock[0].name, ".");
    theCreatedBlock[1].inum = parentInodeNumber;
    strcpy(theCreatedBlock[1].name, "..");

    // Set the rest of the entries to -1
    for (int i = 2; i < THE_ENTRIES_PER_BLOCK_CONSTANT; ++i)
      theCreatedBlock[i].inum = -1;

    // Set the appropriate size
    theInodes[theAvailableInodeNumber].size = 2 * sizeof(dir_ent_t);

    // Set the block in the inode
    theInodes[theAvailableInodeNumber].direct[0] = availableCreatedDataBlockNumber;
  }
  else {
    // This is a file, it's empty
    theInodes[theAvailableInodeNumber].size = 0;
  }
  /* #endregion */

  // Now we need to add an entry
  int theEntriesBlockNumber = -1;
  dir_ent_t theEntriesBlock[THE_ENTRIES_PER_BLOCK_CONSTANT];
  /* #region */
  // Check if we need to create a new block
  if (parentInode.size % UFS_BLOCK_SIZE == 0) {
    // Need to create a new data block
    theEntriesBlock[0].inum = theAvailableInodeNumber;
    strcpy(theEntriesBlock[0].name, name.c_str());

    for (int i = 1; i < THE_ENTRIES_PER_BLOCK_CONSTANT; ++i)
      theEntriesBlock[i].inum = -1;

    // Read bitmap to find the first available data block to write to
    const int theEntriesBlockBit = getFirstAvailableBit(theDataBitmap, theLenOfDataBitmapArr);
    if (theEntriesBlockBit < -1 || theEntriesBlockBit >= super_block.num_data)
      return -ENOTENOUGHSPACE;
    setBit(theDataBitmap, theEntriesBlockBit);
    theEntriesBlockNumber = dataBitToBlockNum(super_block, theEntriesBlockBit);

    // Add the new block to our parent
    const int theIdxToAppendToParentInodeDirect = parentInode.size / UFS_BLOCK_SIZE;
    parentInode.direct[theIdxToAppendToParentInodeDirect] = theEntriesBlockNumber;
  }
  else {
    const int theDirectSize = ceilDiv(parentInode.size, UFS_BLOCK_SIZE);
    const int theLastBlockIdx = theDirectSize - 1;
    theEntriesBlockNumber = parentInode.direct[theLastBlockIdx];
    
    // Read the last block
    disk->readBlock(theEntriesBlockNumber, theEntriesBlock);

    // Now add it to the end of all the previous entries
    const int theIdxToAddToInEntriesBlock = parentInode.size % UFS_BLOCK_SIZE;
    theEntriesBlock[theIdxToAddToInEntriesBlock].inum = theAvailableInodeNumber;
    strcpy(theEntriesBlock[theIdxToAddToInEntriesBlock].name, name.c_str());
  }

  // Update the size of the parentInode
  parentInode.size += sizeof(dir_ent_t);
  /* #endregion */

  // Now we need to write the changes to the disk (inode bitmap, data bitmap, inode region, any data blocks)
  /* #region */
  this->disk->beginTransaction();
  
  this->writeInodeBitmap(&super_block, theInodeBitmap);
  this->writeDataBitmap(&super_block, theDataBitmap);
  this->writeInodeRegion(&super_block, theInodes);

  this->disk->writeBlock(theEntriesBlockNumber, theEntriesBlock);
  if (availableCreatedDataBlockNumber != -1)
    this->disk->writeBlock(availableCreatedDataBlockNumber, theCreatedBlock);
  
  this->disk->commit();
  /* #endregion */

  return theAvailableInodeNumber;
}

int LocalFileSystem::write(int inodeNumber, const void *buffer, int size) {
  super_t super_block;
  readSuperBlock(&super_block);

  // Check input
  /* #region */
  if (!isValidInodeNumber(super_block, inodeNumber))
    return -EINVALIDINODE;

  if (size < 0 || size > MAX_FILE_SIZE)
    return -EINVALIDSIZE;
  /* #endregion */

  // Get the inode region
  inode_t theInodeRegion[super_block.num_inodes];
  readInodeRegion(&super_block, theInodeRegion);

  // Stat the inode
  inode_t & theInodeToWrite = theInodeRegion[inodeNumber];
  /* #region */
  if (theInodeToWrite.type != UFS_REGULAR_FILE)
    return -EINVALIDTYPE;
  /* #endregion */

  // Get data bitmap
  const int theLenOfDataBitmapArr = super_block.data_bitmap_len * UFS_BLOCK_SIZE;
  unsigned char theDataBitmap[theLenOfDataBitmapArr];
  readDataBitmap(&super_block, theDataBitmap);

  // See if we need to allocate or deallocate blocks
  // After this, we will have exactly the number of blocks needed in our inode
  int theNumberOfBlocksPresent = ceilDiv(theInodeToWrite.size, UFS_BLOCK_SIZE);
  int theNumberOfBlocksNeeded = ceilDiv(size, UFS_BLOCK_SIZE);
  int theNumberOfBlocksToAllocate = min(0, theNumberOfBlocksNeeded - theNumberOfBlocksPresent);
  int theNumberOfBlocksToDeallocate = min(0, theNumberOfBlocksPresent - theNumberOfBlocksNeeded);
  /* #region */
  if (theNumberOfBlocksNeeded > DIRECT_PTRS)
    return -ENOTENOUGHSPACE;

  for (int i = 0; i < theNumberOfBlocksToAllocate; ++i) {
    int idx = theNumberOfBlocksPresent + i; // Append at the end
    
    // Get the first available data block number, and set that bit
    int theAvailableBitNumber = getFirstAvailableBit(theDataBitmap, theLenOfDataBitmapArr);
    if (theAvailableBitNumber < 0 || theAvailableBitNumber >= super_block.num_data)
      return -ENOTENOUGHSPACE;
    setBit(theDataBitmap, theAvailableBitNumber);

    // Set the available block number in inode
    theInodeToWrite.direct[idx] = dataBitToBlockNum(super_block, theAvailableBitNumber);
  }

  for (int i = 1; i <= theNumberOfBlocksToDeallocate; ++i) {
    const int idx = theNumberOfBlocksPresent - i; // Start deallocating from the end
    const int theBlockNumberToFree = theInodeToWrite.direct[idx];
    const int theBitToFree = dataBlockNumToBit(super_block, theBlockNumberToFree);

    // Clear the bit to deallocate
    clearBit(theDataBitmap, theBitToFree);
  }
  /* #endregion */

  // Now set the size in inode to reflect what we are about to write
  theInodeToWrite.size = size;

  // Get the size of the last block
  const int theSizeOnTheLastBlock = (theInodeToWrite.size - 1) % UFS_BLOCK_SIZE + 1;

  // Do write
  /* #region */
  disk->beginTransaction();
  
  writeInodeRegion(&super_block, theInodeRegion);
  writeDataBitmap(&super_block, theDataBitmap);

  // Write to all the blocks
  for (int i = 0; i < theNumberOfBlocksNeeded; ++i) {
    const bool isLastBlock = i == theNumberOfBlocksNeeded - 1;
    const int sizeToCopy = isLastBlock ? theSizeOnTheLastBlock : UFS_BLOCK_SIZE;
    
    const int theBufferOffset = i * UFS_BLOCK_SIZE;
    const int theBlockNumber = theInodeToWrite.direct[i];

    unsigned char theBlockContent[UFS_BLOCK_SIZE];
    memcpy(theBlockContent, (unsigned char *)buffer + theBufferOffset, sizeToCopy);

    disk->writeBlock(theBlockNumber, theBlockContent);
  }

  disk->commit();
  /* #endregion */

  return size;
}

int LocalFileSystem::unlink(int parentInodeNumber, string name) {
  super_t super_block;
  readSuperBlock(&super_block);

  // Check inputs
  /* #region */
  if (!isValidInodeNumber(super_block, parentInodeNumber))
    return -EINVALIDINODE;

  if (!isValidName(name))
    return -EINVALIDNAME;

  if (!unlinkAllowed(name))
    return -EUNLINKNOTALLOWED;
  /* #endregion */

  // Get all the inodes
  const int theNumberOfInodes = super_block.num_inodes;
  inode_t theInodeRegion[theNumberOfInodes];
  readInodeRegion(&super_block, theInodeRegion);

  // Get parent inode
  inode_t & theParentInode = theInodeRegion[parentInodeNumber];
  /* #region */
  if (theParentInode.type != UFS_DIRECTORY)
    return -EINVALIDINODE;
  /* #endregion*/

  // Get the data bitmap
  const int theSizeOfDataBitmapArr = super_block.data_bitmap_len * UFS_BLOCK_SIZE;
  unsigned char theDataBitmap[theSizeOfDataBitmapArr];
  readDataBitmap(&super_block, theDataBitmap);

  // Get the inode bitmap
  const int theSizeOfInodeBitmapArr = super_block.inode_bitmap_len * UFS_BLOCK_SIZE;
  unsigned char theInodeBitmap[theSizeOfInodeBitmapArr];
  readInodeBitmap(&super_block, theInodeBitmap);

  // Load all the valid directory entries into a vector
  const int theNumberOfEntries = theParentInode.size / sizeof(dir_ent_t);
  vector<dir_ent_t> theParentEntries(theNumberOfEntries);
  read(parentInodeNumber, theParentEntries.data(), theParentInode.size);

  // Now find the entry to delete
  int parentEntryIdxToDelete = -1;
  /* #region */
  for (int i = 0; i < (int)theParentEntries.size(); ++i) {
    if (string(theParentEntries[i].name) == name) {
      parentEntryIdxToDelete = i;
      break;
    }
  }

  if (parentEntryIdxToDelete < 0)
    return 0; // Did not find index to delete
  /* #endregion */

  // Now delete the contents of the inode in the entry
  const int theInodeBitToDelete = theParentEntries[parentEntryIdxToDelete].inum;
  inode_t theInodeToDelete = theInodeRegion[theInodeBitToDelete];
  /* #region */
  // Make sure dir is empty if applicable
  if (theInodeToDelete.type == UFS_DIRECTORY && theInodeToDelete.size > (int)sizeof(dir_ent_t) * 2)
    return -EDIRNOTEMPTY;

  // Now delete it's contents
  int theNumberOfBlocksToDelete = ceilDiv(theInodeToDelete.size, UFS_BLOCK_SIZE);
  for (int i = 0; i < theNumberOfBlocksToDelete; ++i) {
    const int theDataBlockNumberToDelete = theInodeToDelete.direct[i];
    const int theDataBitToDelete = dataBlockNumToBit(super_block, theDataBlockNumberToDelete);
    clearBit(theDataBitmap, theDataBitToDelete);
  }
  /* #endregion */

  // Now delete the inode itself
  clearBit(theInodeBitmap, theInodeBitToDelete);  

  // Now delete the actual directory in the parent data
  theParentEntries.erase(next(theParentEntries.begin(), parentEntryIdxToDelete));
  theParentInode.size -= sizeof(dir_ent_t);
  
  // Delete the last block if after the erase we don't need it
  if (theParentInode.size % UFS_BLOCK_SIZE == 0) {
    const int theNumberOfBlocksNeeded = theParentInode.size / UFS_BLOCK_SIZE;
    
    if (theNumberOfBlocksNeeded < DIRECT_PTRS) {
      // Only delete if there is something to delete
      const int theBlockNumberToDelete = theParentInode.direct[theNumberOfBlocksNeeded];
      const int theDataBitToDelete = dataBlockNumToBit(super_block, theBlockNumberToDelete);
      clearBit(theDataBitmap, theDataBitToDelete);
      theParentInode.direct[theNumberOfBlocksNeeded] = -1;
    }
  }

  // Make sure we set the proper identifiers to mark end in a block of entries
  while (theParentEntries.size() * sizeof(dir_ent_t) % UFS_BLOCK_SIZE != 0) {
    // The next entry has to be -1
    dir_ent_t theEmptyEntry;
    theEmptyEntry.inum = -1;
    theParentEntries.push_back(theEmptyEntry);
  }

  // Now we need to write all changes
  /* #region */
  disk->beginTransaction();
  writeInodeRegion(&super_block, theInodeRegion);
  writeDataBitmap(&super_block, theDataBitmap);
  writeInodeBitmap(&super_block, theInodeBitmap);

  for (int i = 0; i < ceilDiv(theParentInode.size, UFS_BLOCK_SIZE); ++i) {
    const int offset = i * THE_ENTRIES_PER_BLOCK_CONSTANT;
    disk->writeBlock(theParentInode.direct[i], theParentEntries.data() + offset);
  }

  disk->commit();
  /* #endregion */

  return 0; // Done
}

bool LocalFileSystem::diskHasSpace(super_t *super, int numInodesNeeded, int numDataBytesNeeded, int numDataBlocksNeeded) {
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
  for (int i = 0; i < super->data_bitmap_len; ++i) {
    int theBlockToRead = super->data_bitmap_addr + i;
    auto theBufferStart = dataBitmap + i * UFS_BLOCK_SIZE;
    disk->readBlock(theBlockToRead, theBufferStart);
  }
}

void LocalFileSystem::writeDataBitmap(super_t *super, unsigned char *dataBitmap) {
  for (int i = 0; i < super->data_bitmap_len; ++i) {
    int theBlockToRead = super->data_bitmap_addr + i;
    auto theBufferStart = dataBitmap + i * UFS_BLOCK_SIZE;
    disk->writeBlock(theBlockToRead, theBufferStart);
  }
}

void LocalFileSystem::readInodeRegion(super_t *super, inode_t *inodes) {
  for (int i = 0; i < super->inode_region_len; ++i) {
    int theBlockToRead = super->inode_region_addr + i;
    auto theBufferStart = inodes + i * THE_INODES_PER_BLOCK_CONSTANT;
    disk->readBlock(theBlockToRead, theBufferStart);
  }
}

void LocalFileSystem::writeInodeRegion(super_t *super, inode_t *inodes) {
  for (int i = 0; i < super->inode_region_len; ++i) {
    int theBlockToRead = super->inode_region_addr + i;
    auto theBufferStart = inodes + i * THE_INODES_PER_BLOCK_CONSTANT;
    disk->writeBlock(theBlockToRead, theBufferStart);
  }
}

/* #endregion Class functions */