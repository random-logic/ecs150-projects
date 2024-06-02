#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

int main(int argc, char *argv[]) {
  if (argc != 2) {
    cout << argv[0] << ": diskImageFile" << endl;
    return 1;
  }

  string theNameOfTheDiskImage(argv[1]);
  Disk theDisk(theNameOfTheDiskImage, UFS_BLOCK_SIZE);
  LocalFileSystem theLocalFileSystem(&theDisk);

  super_t theSuperBlock;
  theLocalFileSystem.readSuperBlock(&theSuperBlock);

  printContentsOfAllDirectories(theSuperBlock.data_region_addr, theLocalFileSystem);
}

void printContentsOfAllDirectories(const int theInodeNumberToRead, LocalFileSystem &theLocalFileSystem) {
  inode_t theInode;
  theLocalFileSystem.stat(theInodeNumberToRead, &theInode);
  
  const int theNumberOfEntries = theInode.size / sizeof(dir_ent_t);
  dir_ent_t theEntries[theNumberOfEntries];
  theLocalFileSystem.read(theInodeNumberToRead, theEntries, theNumberOfEntries * sizeof(dir_ent_t));

  for (dir_ent_t entry : theEntries) {
    cout << entry.name << endl;
  }

  for (dir_ent_t entry : theEntries) {
    if (entry.name == ".." || entry.name == ".")
      continue;

    printContentsOfAllDirectories(entry.inum, theLocalFileSystem);
  }
}