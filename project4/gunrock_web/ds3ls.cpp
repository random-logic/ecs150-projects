#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

#include <vector>

using namespace std;

const int THE_MAX_NUMBER_OF_ENTRIES_CONSTANT = MAX_FILE_SIZE / sizeof(dir_ent_t);
const int THE_ROOT_INODE_NUMBER_CONSTANT = 0;

bool cmp(const dir_ent_t & a, const dir_ent_t & b) {
  return strcmp(a.name, b.name) < 0;
}

void printContentsOfAllDirectories(const int inodeNum, LocalFileSystem &fs, string path = "/") {
  cout << "Directory " << path << endl;
  
  inode_t theInode;

  if (fs.stat(inodeNum, &theInode))
    cout << "stat err" << endl;
  
  const int theNumberOfEntries = theInode.size / sizeof(dir_ent_t);
  dir_ent_t theEntries[theNumberOfEntries];
  if (fs.read(inodeNum, theEntries, theInode.size) < 0)
    return;

  sort(theEntries, theEntries + theNumberOfEntries, cmp);

  for (const dir_ent_t & entry : theEntries) {
    cout << entry.inum << "\t" << entry.name << endl;
  }
  cout << endl;

  for (const dir_ent_t entry : theEntries) {
    string theNameOfEntry(entry.name);
    if (theNameOfEntry == ".." || theNameOfEntry == ".")
      continue;

    inode_t theNextInode;
    if (fs.stat(entry.inum, &theNextInode))
      continue;

    if (theNextInode.type != UFS_DIRECTORY)
      continue;

    string theDirPath = path + theNameOfEntry + "/";
    printContentsOfAllDirectories(entry.inum, fs, theDirPath);
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