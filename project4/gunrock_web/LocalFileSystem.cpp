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

  // TODO: ??

  return 0;
}

int LocalFileSystem::stat(int inodeNumber, inode_t *inode) {
  super_t super_block;
  readSuperBlock(&super_block);

  if (inodeNumber < 0 || inodeNumber >= super_block.num_inodes)
    return -EINVALIDINODE;

  // TODO: ??

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
