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

  cout << "Super" << endl;
  cout << "inode_region_addr " << theSuperBlock.inode_region_addr << endl;
  cout << "data_region_addr " << theSuperBlock.data_region_addr << endl;
  cout << endl;

  const int theSizeOfInodeBitmapArr = theSuperBlock.inode_bitmap_len * UFS_BLOCK_SIZE;
  unsigned char theInodeBitmap[theSizeOfInodeBitmapArr];
  theLocalFileSystem.readInodeBitmap(&theSuperBlock, theInodeBitmap);

  cout << "Inode bitmap" << endl;
  for (unsigned char byte : theInodeBitmap) {
    cout << (unsigned int) byte << " ";
  }
  cout << endl << endl;

  const int theSizeOfDataBitmapArr = theSuperBlock.data_bitmap_len * UFS_BLOCK_SIZE;
  unsigned char theDataBitmap[theSizeOfDataBitmapArr];
  theLocalFileSystem.readDataBitmap(&theSuperBlock, theDataBitmap);

  cout << "Data bitmap" << endl;
  for (unsigned char byte : theDataBitmap) {
    cout << (unsigned int) byte << " ";
  }
  cout << endl;
}
