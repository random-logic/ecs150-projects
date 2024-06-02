#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

bool cmp(const dir_ent_t & a, const dir_ent_t & b) {
  return strcmp(a.name, b.name);
}

void printContentsOfAllDirectories(const int theInodeNumberToRead, LocalFileSystem &theLocalFileSystem, string name = "/") {
  cout << "Directory " << name << endl;
  
  inode_t theInode;
  theLocalFileSystem.stat(theInodeNumberToRead, &theInode);
  
  const int theNumberOfEntries = theInode.size / sizeof(dir_ent_t);
  dir_ent_t theEntries[theNumberOfEntries];
  theLocalFileSystem.read(theInodeNumberToRead, theEntries, theNumberOfEntries * sizeof(dir_ent_t));

  sort(theEntries, theEntries + theNumberOfEntries, cmp);

  for (dir_ent_t entry : theEntries) {
    cout << entry.inum << "\t" << entry.name << endl;
  }

  for (dir_ent_t entry : theEntries) {
    string theNameOfEntry(entry.name);
    if (theNameOfEntry == ".." || theNameOfEntry == ".")
      continue;

    printContentsOfAllDirectories(entry.inum, theLocalFileSystem, theNameOfEntry);
  }
}

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