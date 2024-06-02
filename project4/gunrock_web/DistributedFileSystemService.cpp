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
  int theParentInodeNumber;
  int theInodeNumber = theSuperBlock.inode_region_addr;
  string theEntryName;
  if (!getOrCreateToInode(theParentInodeNumber, theInodeNumber, theEntryName, thePaths, this->fileSystem)) {
    // This is ClientError::badRequest()
    // TODO Set ClientError::badRequest() ??
    return;
  }

  // Now read the file
  inode_t theInode;
  this->fileSystem->stat(theInodeNumber, &theInode);
  const int theNumberOfBytesInFile = theInode.size;
  char theFileBuffer[theNumberOfBytesInFile + 1];
  #pragma region
  if (this->fileSystem->read(theInodeNumber, theFileBuffer, theNumberOfBytesInFile)) {
    // TODO: Handle error that happened during the read ??
    response->setBody("");
    return;
  }

  theFileBuffer[theNumberOfBytesInFile] = '\0'; // Ensure null termination
  #pragma endregion

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
  
  // Get to the right path


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

  // Go to the correct file / directory before deleting
  int theParentInodeNumber = -1;
  int theInodeNumber = theSuperBlock.inode_region_addr;
  string theEntryName, buffer;
  #pragma region
  while (getline(thePaths, buffer, '/')) {
    // Set the proper variables
    theEntryName = buffer;

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

    // Now find the next entry
    bool found = false;
    for (dir_ent_t entry : theEntries) {
      if (string(entry.name) == theEntryName) {
        found = true;
        theParentInodeNumber = theInodeNumber;
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
  #pragma region
  // It's an error if there is no parent directory to delete from
  if (theParentInodeNumber == -1) {
    // TODO: Get error ??
    return;
  }

  if (this->fileSystem->unlink(theParentInodeNumber, theEntryName)) {
    // TODO Set the error that is thrown from unlink ??
    return;
  }
  #pragma endregion

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

// Returns true if it was successful, false if it was a ClientError::badRequest()
bool getOrCreateToInode(int & theParentInodeNumber, int & theInodeNumber, string & theEntryName, istringstream & thePaths, LocalFileSystem * theLocalFileSystem) {
  // ?? TODO - I think this has a bug, Imma redo this abstraction   
  theParentInodeNumber = -1;
  string buffer;
  while (getline(thePaths, buffer, '/')) {
    // Set the entry name to what is next
    theEntryName = buffer;
    
    // Get the corresponding inode of the current directory 
    inode_t theCurrentInode;
    theLocalFileSystem->stat(theInodeNumber, &theCurrentInode);

    // If this is not a directory, this is a ClientError::badRequest()
    if (theCurrentInode.type != UFS_DIRECTORY) {
      return false;
    }

    // Read entries
    const int theNumberOfEntries = theCurrentInode.size / sizeof(dir_ent_t);
    dir_ent_t theEntries[theNumberOfEntries];
    theLocalFileSystem->read(theInodeNumber, theEntries, theNumberOfEntries * sizeof(dir_ent_t));

    // Now switch to the next inode number
    bool found = false;
    for (dir_ent_t entry : theEntries) {
      if (string(entry.name) == theEntryName) {
        found = true;
        theParentInodeNumber = theInodeNumber;
        theInodeNumber = entry.inum;
        break; // Done
      }
    }

    // Not found, then create a new corresponding entry
    if (!found) {
      theParentInodeNumber = theInodeNumber;
      theInodeNumber = theLocalFileSystem->create(theInodeNumber, UFS_DIRECTORY, theEntryName);
    }
  }

  return true;
}