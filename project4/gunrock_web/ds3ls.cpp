#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

#include <vector>

using namespace std;

const int THE_ROOT_INODE_NUMBER_CONSTANT = 3;

bool cmp(const dir_ent_t & a, const dir_ent_t & b) {
  return strcmp(a.name, b.name);
}

void printContentsOfAllDirectories(const int theInodeNumberToRead, LocalFileSystem &theLocalFileSystem, string name = "/") {
  cout << "Directory " << name << endl;
  
  inode_t theInode;

  if (theLocalFileSystem.stat(theInodeNumberToRead, &theInode))
    cout << "err" << endl;
  
  cout << theInode.type << endl;
  cout << theInode.size / UFS_BLOCK_SIZE << endl;

  for (int inodeNumber : theInode.direct) {
    cout << inodeNumber << " ";
  }

  cout << endl;
  return;
  
  const int theNumberOfEntries = theInode.size / sizeof(dir_ent_t);
  vector<dir_ent_t> theEntries(theNumberOfEntries);
  theLocalFileSystem.read(theInodeNumberToRead, theEntries.data(), MAX_FILE_SIZE);

  sort(theEntries.begin(), theEntries.end(), cmp);

  for (const dir_ent_t & entry : theEntries) {
    cout << entry.inum << "\t" << entry.name << endl;
  }

  for (const dir_ent_t & entry : theEntries) {
    string theNameOfEntry(entry.name);
    if (theNameOfEntry == ".." || theNameOfEntry == ".")
      continue;

    if (name != "/")
      name.push_back('/');
    theNameOfEntry = name + theNameOfEntry;
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

  printContentsOfAllDirectories(THE_ROOT_INODE_NUMBER_CONSTANT, theLocalFileSystem);
}