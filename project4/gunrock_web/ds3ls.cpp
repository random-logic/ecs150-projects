#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

#include <vector>

using namespace std;

const int THE_ROOT_INODE_NUMBER_CONSTANT = 0;

bool cmp(const dir_ent_t & a, const dir_ent_t & b) {
  return strcmp(a.name, b.name) < 0;
}

void printContentsOfAllDirectories(const int theInodeNumberToRead, LocalFileSystem &theLocalFileSystem, string name = "/") {
  cout << "Directory " << name << endl;
  
  inode_t theInode;

  if (theLocalFileSystem.stat(theInodeNumberToRead, &theInode))
    cout << "stat err" << endl;
  
  vector<dir_ent_t> theEntries(MAX_FILE_SIZE / sizeof(dir_ent_t));
  const int theNumberOfBytesRead = theLocalFileSystem.read(theInodeNumberToRead, theEntries.data(), UFS_BLOCK_SIZE);

  if (theNumberOfBytesRead < 0)
    cout << "read err" << endl;

  int theActualNumberOfEntries = theNumberOfBytesRead / (int)sizeof(dir_ent_t);

  theEntries.resize(theActualNumberOfEntries);

  sort(theEntries.begin(), theEntries.end(), cmp);

  for (const dir_ent_t & entry : theEntries) {
    cout << entry.inum << "\t" << entry.name << endl;
  }
  cout << endl;

  for (const dir_ent_t & entry : theEntries) {
    string theNameOfEntry(entry.name);
    if (theNameOfEntry == ".." || theNameOfEntry == ".")
      continue;

    inode_t theNextInode;
    theLocalFileSystem.stat(entry.inum, &theNextInode);

    if (theNextInode.type != UFS_DIRECTORY)
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