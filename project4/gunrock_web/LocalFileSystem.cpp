#include <iostream>
#include <string>
#include <vector>
#include <assert.h>

#include "LocalFileSystem.h"
#include "ufs.h"

#include <cstring>

using namespace std;


LocalFileSystem::LocalFileSystem(Disk *disk) {
  this->disk = disk;
}

void LocalFileSystem::readSuperBlock(super_t *super) {
  char buffer[UFS_BLOCK_SIZE];
  this->disk->readBlock(0, buffer);
  std::memcpy(super, buffer, sizeof(super_t));
}

int LocalFileSystem::lookup(int parentInodeNumber, string name) {
  super_t super_block;
  readSuperBlock(&super_block);

  if (parentInodeNumber < 0 || parentInodeNumber >= super_block.num_inodes)
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

  if (inodeNumber < 0 || inodeNumber >= super_block.num_inodes)
    return -EINVALIDINODE;

  int theBlockToRead = super_block.inode_region_addr + inodeNumber * sizeof(inode_t) / UFS_BLOCK_SIZE;
  int theInodeOffset = inodeNumber * sizeof(inode_t) % UFS_BLOCK_SIZE;

  char buffer[UFS_BLOCK_SIZE];
  this->disk->readBlock(theBlockToRead, buffer);

  std::memcpy(inode, buffer + theInodeOffset, sizeof(inode_t));

  return 0;
}

int LocalFileSystem::read(int inodeNumber, void *buffer, int size) {
  super_t super_block;
  readSuperBlock(&super_block);

  if (inodeNumber < 0 || inodeNumber >= super_block.num_inodes)
    return -EINVALIDINODE;

  // TODO: ??

  return 0;
}

int LocalFileSystem::create(int parentInodeNumber, int type, string name) {
  super_t super_block;
  readSuperBlock(&super_block);

  if (parentInodeNumber < 0 || parentInodeNumber >= super_block.num_inodes)
    return -EINVALIDINODE;

  // TODO: ??

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

  if (parentInodeNumber < 0 || parentInodeNumber >= super_block.num_inodes)
    return -EINVALIDINODE;

  // TODO: ??

  return 0;
}

bool LocalFileSystem::diskHasSpace(super_t *super, int numInodesNeeded, int numDataBytesNeeded, int numDataBlocksNeeded=0) {
  // ??
}

void LocalFileSystem::readInodeBitmap(super_t *super, unsigned char *inodeBitmap) {  
  int theTotalSizeOfBitmap = super->inode_bitmap_len * UFS_BLOCK_SIZE;
  
  for (int i = 0; i < super->inode_bitmap_len; ++i) {
    int theBlockToRead = super->inode_bitmap_addr + i;
    auto theBufferStart = inodeBitmap + i * UFS_BLOCK_SIZE;
    disk->readBlock(theBlockToRead, theBufferStart);
  }
}

void LocalFileSystem::writeInodeBitmap(super_t *super, unsigned char *inodeBitmap) {
  int theTotalSizeOfBitmap = super->inode_bitmap_len * UFS_BLOCK_SIZE;
  
  for (int i = 0; i < super->inode_bitmap_len; ++i) {
    int theBlockToRead = super->inode_bitmap_addr + i;
    auto theBufferStart = inodeBitmap + i * UFS_BLOCK_SIZE;
    disk->writeBlock(theBlockToRead, theBufferStart);
  }
}

void LocalFileSystem::readDataBitmap(super_t *super, unsigned char *dataBitmap) {
  int theTotalSizeOfBitmap = super->data_bitmap_len * UFS_BLOCK_SIZE;

  for (int i = 0; i < super->inode_bitmap_len; ++i) {
    int theBlockToRead = super->data_bitmap_addr + i;
    auto theBufferStart = dataBitmap + i * UFS_BLOCK_SIZE;
    disk->readBlock(theBlockToRead, theBufferStart);
  }
}

void LocalFileSystem::writeDataBitmap(super_t *super, unsigned char *dataBitmap) {
  int theTotalSizeOfBitmap = super->data_bitmap_len * UFS_BLOCK_SIZE;

  for (int i = 0; i < super->inode_bitmap_len; ++i) {
    int theBlockToRead = super->data_bitmap_addr + i;
    auto theBufferStart = dataBitmap + i * UFS_BLOCK_SIZE;
    disk->writeBlock(theBlockToRead, theBufferStart);
  }
}

void LocalFileSystem::readInodeRegion(super_t *super, inode_t *inodes) {
  int theTotalSizeOfBitmap = super->inode_region_len * UFS_BLOCK_SIZE;

  for (int i = 0; i < super->inode_region_len; ++i) {
    int theBlockToRead = super->inode_region_addr + i;
    auto theBufferStart = inodes + i * UFS_BLOCK_SIZE;
    disk->readBlock(theBlockToRead, theBufferStart);
  }
}

void LocalFileSystem::writeInodeRegion(super_t *super, inode_t *inodes) {
  int theTotalSizeOfBitmap = super->inode_region_len * UFS_BLOCK_SIZE;

  for (int i = 0; i < super->inode_region_len; ++i) {
    int theBlockToRead = super->inode_region_addr + i;
    auto theBufferStart = inodes + i * UFS_BLOCK_SIZE;
    disk->writeBlock(theBlockToRead, theBufferStart);
  }
}