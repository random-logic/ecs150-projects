#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;


int main(int argc, char *argv[]) {
  if (argc != 3) {
    cout << argv[0] << ": diskImageFile inodeNumber" << endl;
    return 1;
  }

  Disk theDisk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem theLocalFileSystem(&theDisk);

  super_t theSuperBlock;
  theLocalFileSystem.readSuperBlock(&theSuperBlock);

  int theInodeNumber = atoi(argv[2]);
  inode_t theInode;
  theLocalFileSystem.stat(theInodeNumber, &theInode);

  cout << "File blocks" << endl;
  for (int i = 0; i < (theInode.size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE; ++i) {
    cout << theInode.direct[i] << endl;
  }
  cout << endl;

  cout << "File data" << endl;
  char fileData[MAX_FILE_SIZE];
  int bytesRead = theLocalFileSystem.read(theInodeNumber, fileData, MAX_FILE_SIZE);
  
  for (int i = 0; i < bytesRead; ++i)
    cout << fileData[i];

  cout << endl;
}
