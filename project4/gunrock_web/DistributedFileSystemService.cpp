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

#include <string.h>

using namespace std;

const int THE_ROOT_INODE_NUMBER_OF_FS_CONSTANT = 0;

/* #region Helpers */
bool cmp(const dir_ent_t a, const dir_ent_t b) {
  return strcmp(a.name, b.name) < 0;
}

void setConflict(HTTPResponse * response) {
  response->setStatus(ClientError::conflict().status_code);
  response->setBody(ClientError::conflict().what());
}

void setNotEnoughSpace(HTTPResponse * response) {
  response->setStatus(ClientError::insufficientStorage().status_code);
  response->setBody(ClientError::insufficientStorage().what());
}

void setNotFound(HTTPResponse * response) {
  response->setStatus(ClientError::notFound().status_code);
  response->setBody(ClientError::notFound().what());
}

void setBadRequest(HTTPResponse * response) {
  response->setStatus(ClientError::badRequest().status_code);
  response->setBody(ClientError::badRequest().what());
}

bool parsePath(const string & theUrl, istringstream & thePaths) {
  const string root = "ds3/";
  const int theStartingIndexOfRoot = theUrl.find(root);
  if (theStartingIndexOfRoot == static_cast<const int>(string::npos))
    return false;
  
  string allPaths = theUrl.substr(theStartingIndexOfRoot + root.size());
  thePaths = istringstream(allPaths);
  return true;
}

bool getOrCreateToInode(int & theParentInodeNumber, int & theInodeNumber, string & theEntryName, istringstream & thePaths, LocalFileSystem * theLocalFileSystem);
/* #endregion Helpers */

DistributedFileSystemService::DistributedFileSystemService(string diskFile) : HttpService("/ds3/") {
  fileSystem = new LocalFileSystem(new Disk(diskFile, UFS_BLOCK_SIZE));
}  

void DistributedFileSystemService::get(HTTPRequest *request, HTTPResponse *response) {  
  // Get the paths to look up in the file system in order
  const string theUrl = request->getUrl();
  istringstream thePaths;
  if (!parsePath(theUrl, thePaths)) {
    // Nothing to read if the path did not parse
    response->setBody("");
    return;
  }

  // Go to the correct inode before reading
  int theInodeNumberToRead = THE_ROOT_INODE_NUMBER_OF_FS_CONSTANT;
  string bufferNextEntryName;
  while (getline(thePaths, bufferNextEntryName, '/')) { 
    const int theNextInode = fileSystem->lookup(theInodeNumberToRead, bufferNextEntryName);
    if (theNextInode == -ENOTFOUND) {
      setNotFound(response);
      return;
    }
    else if (theNextInode < 0) {
      setBadRequest(response);
      return;
    }
    
    theInodeNumberToRead = theNextInode;
  }
  
  // Get stats of the inode to read
  inode_t theInodeToRead;
  fileSystem->stat(theInodeNumberToRead, &theInodeToRead);

  // Read the contents of the inode number
  /* #region */
  if (theInodeToRead.type == UFS_REGULAR_FILE) {
    // Just print the contents of the file
    char buffer[theInodeToRead.size + 1];
    if (fileSystem->read(theInodeNumberToRead, buffer, theInodeToRead.size) < 0) {
      setBadRequest(response);
      return;
    }

    buffer[theInodeToRead.size] = '\0'; // Ensure null termination

    response->setBody(buffer);
  }
  else {
    // Print all directory entries
    const int numberOfEntries = theInodeToRead.size / sizeof(dir_ent_t);
    dir_ent_t entries[theInodeToRead.size / sizeof(dir_ent_t)];
    if (fileSystem->read(theInodeNumberToRead, entries, theInodeToRead.size) < 0) {
      setBadRequest(response);
      return;
    }

    sort(entries, entries + numberOfEntries, cmp);
    
    string theResult = "";
    for (dir_ent_t entry : entries) {
      string entryName(entry.name);
      if (entryName == "." || entryName == "..")
        continue;

      inode_t theInodeOfEntry;
      fileSystem->stat(entry.inum, &theInodeOfEntry);

      if (theInodeOfEntry.type == UFS_DIRECTORY)
        entryName.push_back('/');

      theResult += entryName + "\n";
    }

    response->setBody(theResult);
  }
  /* #endregion */
}

void DistributedFileSystemService::put(HTTPRequest *request, HTTPResponse *response) {  
  // Get the paths to look up in the file system in order
  const string theUrl = request->getUrl();
  istringstream thePaths;
  if (!parsePath(theUrl, thePaths)) {
    response->setBody("");
    return;
  }
  
  // Convert the paths into a vector
  vector<string> thePathsVec;
  string bufferNextEntryName;
  while (getline(thePaths, bufferNextEntryName, '/')) {
    thePathsVec.push_back(bufferNextEntryName);
  }

  // Get to the right path
  int inodeNumToWrite = THE_ROOT_INODE_NUMBER_OF_FS_CONSTANT;
  for (int i = 0; i < (int)thePathsVec.size(); ++i) {
    const string theNextEntryName = thePathsVec[i];
    int theNextInode = fileSystem->lookup(inodeNumToWrite, theNextEntryName);
    if (theNextInode == -ENOTFOUND) {
      // We have to create it
      const bool isLastInode = i == (int)thePathsVec.size() - 1;
      const bool type = isLastInode ? UFS_REGULAR_FILE : UFS_DIRECTORY;
      theNextInode = fileSystem->create(inodeNumToWrite, type, theNextEntryName);
      if (theNextInode == -ENOTENOUGHSPACE) {
        setNotEnoughSpace(response);
        return;
      }
      else if (theNextInode == -EINVALIDTYPE) {
        setConflict(response);
        return;
      }
      else if (theNextInode < 0) {
        setBadRequest(response);
        return;
      }
    }
    else if (theNextInode < 0) {
      setBadRequest(response);
      return;
    }
    
    inodeNumToWrite = theNextInode;
  }

  // Now write to the inode
  const string contentToWrite = request->getBody();
  const int bytesWritten = fileSystem->write(inodeNumToWrite, contentToWrite.data(), contentToWrite.size());
  /* #region */
  if (bytesWritten == -ENOTENOUGHSPACE || bytesWritten == -EINVALIDSIZE) {
    setNotEnoughSpace(response);
    return;
  }
  else if (bytesWritten < 0) {
    setBadRequest(response);
    return;
  }
  /* #endregion */

  response->setBody("");
}

void DistributedFileSystemService::del(HTTPRequest *request, HTTPResponse *response) {
  response->setStatus(500);
  return;
  
  // Get the paths to look up in the file system in order
  const string theUrl = request->getUrl();
  istringstream thePaths;
  /* #region */
  if (!parsePath(theUrl, thePaths)) {
    response->setBody("");
    return;
  }
  /* #endregion */
  
  // Get the super block
  super_t theSuperBlock;
  fileSystem->readSuperBlock(&theSuperBlock);

  // Go to the correct file / directory before deleting
  int theParentInodeNumber = -1;
  int theInodeNumber = theSuperBlock.inode_region_addr;
  string theEntryName, buffer;
  /* #region */
  while (getline(thePaths, buffer, '/')) {
    // Set the proper variables
    theEntryName = buffer;

    // Get the corresponding inode
    inode_t theCurrentInode;
    fileSystem->stat(theInodeNumber, &theCurrentInode);

    // If this is not a directory, this is an error
    if (theCurrentInode.type != UFS_DIRECTORY) {
      // ?? TODO - set response to ClientError::badRequest()
      return;
    }

    // Read entries
    const int theNumberOfEntries = theCurrentInode.size / sizeof(dir_ent_t);
    dir_ent_t theEntries[theNumberOfEntries];
    fileSystem->read(theInodeNumber, theEntries, theNumberOfEntries * sizeof(dir_ent_t));

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
  /* #endregion */

  // Now delete the file or directory
  /* #region */
  // It's an error if there is no parent directory to delete from
  if (theParentInodeNumber == -1) {
    // TODO: Get error ??
    return;
  }

  if (fileSystem->unlink(theParentInodeNumber, theEntryName)) {
    // TODO Set the error that is thrown from unlink ??
    return;
  }
  /* #endregion */

  response->setBody("");
}

// Returns the inode number of dest if it was successful, -1 if it was a ClientError::badRequest()
bool getOrCreateToInode(int & parentInodeNumber, int & theInodeNumber, string & theEntryName, istringstream & paths, LocalFileSystem * fs) {
  // ?? TODO - I think this has a bug, Imma redo this abstraction   
  parentInodeNumber = -1;
  string buffer;
  while (getline(paths, buffer, '/')) {
    // Set the entry name to what is next
    theEntryName = buffer;
    
    // Get the corresponding inode of the current directory 
    inode_t theCurrentInode;
    fs->stat(theInodeNumber, &theCurrentInode);

    // If this is not a directory, this is a ClientError::badRequest()
    if (theCurrentInode.type != UFS_DIRECTORY) {
      return false;
    }

    // Read entries
    const int theNumberOfEntries = theCurrentInode.size / sizeof(dir_ent_t);
    dir_ent_t theEntries[theNumberOfEntries];
    fs->read(theInodeNumber, theEntries, theNumberOfEntries * sizeof(dir_ent_t));

    // Now switch to the next inode number
    bool found = false;
    for (dir_ent_t entry : theEntries) {
      if (string(entry.name) == theEntryName) {
        found = true;
        parentInodeNumber = theInodeNumber;
        theInodeNumber = entry.inum;
        break; // Done
      }
    }

    // Not found, then create a new corresponding entry
    if (!found) {
      parentInodeNumber = theInodeNumber;
      theInodeNumber = fs->create(theInodeNumber, UFS_DIRECTORY, theEntryName);
    }
  }

  return true;
}