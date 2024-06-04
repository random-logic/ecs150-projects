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
  if (theLocalFileSystem.stat(theInodeNumber, &theInode))
    return -1;

  cout << "File blocks" << endl;
  for (int i = 0; i < (theInode.size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE; ++i) {
    cout << theInode.direct[i] << endl;
  }
  cout << endl;

  cout << "File data" << endl;
  char fileData[theInode.size + 1];
  const int bytesRead = theLocalFileSystem.read(theInodeNumber, fileData, theInode.size);
  if (bytesRead < 0)
    return -1;
  
  fileData[theInode.size] = '\0'; // Ensure null termination
  cout << fileData;
}
