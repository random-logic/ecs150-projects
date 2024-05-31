#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sstream>
#include <iostream>
#include <map>
#include <string>
#include <algorithm>

#include "DistributedFileSystemService.h"
#include "ClientError.h"
#include "ufs.h"
#include "WwwFormEncodedDict.h"

using namespace std;

DistributedFileSystemService::DistributedFileSystemService(string diskFile) : HttpService("/ds3/") {
  this->fileSystem = new LocalFileSystem(new Disk(diskFile, UFS_BLOCK_SIZE));
}  

void DistributedFileSystemService::get(HTTPRequest *request, HTTPResponse *response) {
  // Get the paths to look up in the file system in order
  const string theUrl = request->getUrl();
  istringstream thePaths;
  #pragma region
  if (!parsePath(theUrl, thePaths)) {
    response->setBody("");
    return;
  }
  #pragma endregion

  // Get the super block
  super_t theSuperBlock;
  this->fileSystem->readSuperBlock(&theSuperBlock);

  // Go to the correct file / directory before reading
  int theInodeNumber = theSuperBlock.inode_region_addr;
  string theNextEntryName;
  #pragma region
  while (getline(thePaths, theNextEntryName, '/')) {
    // Get the corresponding inode
    inode_t theCurrentInode;
    this->fileSystem->stat(theInodeNumber, &theCurrentInode);

    // If this is not a directory, this is an error
    if (theCurrentInode.type != UFS_DIRECTORY) {
      // ?? TODO - set response to ClientError::badRequest()
      return;
    }

    // Read entries
    const int theNumberOfEntries = theCurrentInode.size / sizeof(dir_ent_t);
    dir_ent_t theEntries[theNumberOfEntries];
    this->fileSystem->read(theInodeNumber, theEntries, theNumberOfEntries * sizeof(dir_ent_t));

    // Now switch to the new inode number
    bool found = false;
    for (dir_ent_t entry : theEntries) {
      if (string(entry.name) == theNextEntryName) {
        found = true;
        theInodeNumber = entry.inum;
        break; // Done
      }
    }

    // Not found, return an error
    if (!found) {
      // TODO - set response to ClientError::notFound()
      return;
    }
  }
  #pragma endregion

  // Now read the file
  inode_t theInode;
  this->fileSystem->stat(theInodeNumber, &theInode);
  const int theNumberOfBytesInFile = theInode.size;
  char theFileBuffer[theNumberOfBytesInFile + 1];
  this->fileSystem->read(theInodeNumber, theFileBuffer, theNumberOfBytesInFile);
  theFileBuffer[theNumberOfBytesInFile] = '\0'; // Ensure null termination

  response->setBody(string(theFileBuffer));
}

void DistributedFileSystemService::put(HTTPRequest *request, HTTPResponse *response) {
  // Get the paths to look up in the file system in order
  const string theUrl = request->getUrl();
  istringstream thePaths;
  #pragma region
  if (!parsePath(theUrl, thePaths)) {
    response->setBody("");
    return;
  }
  #pragma endregion
  
  // TODO ??
  // How to get the contents of the file when put?

  response->setBody("");
}

void DistributedFileSystemService::del(HTTPRequest *request, HTTPResponse *response) {
  // Get the paths to look up in the file system in order
  const string theUrl = request->getUrl();
  istringstream thePaths;
  #pragma region
  if (!parsePath(theUrl, thePaths)) {
    response->setBody("");
    return;
  }
  #pragma endregion
  
  // Get the super block
  super_t theSuperBlock;
  this->fileSystem->readSuperBlock(&theSuperBlock);

  // TODO ??

  // Go to the correct file / directory before deleting
  int theInodeNumber = theSuperBlock.inode_region_addr;
  string theNextEntryName;
  #pragma region
  while (getline(thePaths, theNextEntryName, '/')) {
    // Get the corresponding inode
    inode_t theCurrentInode;
    this->fileSystem->stat(theInodeNumber, &theCurrentInode);

    // If this is not a directory, this is an error
    if (theCurrentInode.type != UFS_DIRECTORY) {
      // ?? TODO - set response to ClientError::badRequest()
      return;
    }

    // Read entries
    const int theNumberOfEntries = theCurrentInode.size / sizeof(dir_ent_t);
    dir_ent_t theEntries[theNumberOfEntries];
    this->fileSystem->read(theInodeNumber, theEntries, theNumberOfEntries * sizeof(dir_ent_t));

    // Now switch to the new inode number
    bool found = false;
    for (dir_ent_t entry : theEntries) {
      if (string(entry.name) == theNextEntryName) {
        found = true;
        theInodeNumber = entry.inum;
        break; // Done
      }
    }

    // Not found, then we are done
    if (!found) {
      // Nothing to do
      response->setBody("");
      return;
    }
  }
  #pragma endregion

  // Now delete the file or directory
  // ??

  response->setBody("");
}

bool parsePath(const string & theUrl, istringstream & thePaths) {
  const string root = "ds3/";
  const int theStartingIndexOfRoot = theUrl.find(root);
  istringstream thePaths;
  if (theStartingIndexOfRoot == string::npos) {
    return false;
  }
  else {
    string allPaths = theUrl.substr(theStartingIndexOfRoot + root.size());
    thePaths = istringstream(allPaths);
  }
}