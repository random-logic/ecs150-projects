#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>

#include "FileService.h"

using namespace std;

// constructor for fileservice
FileService::FileService(string basedir) : HttpService("/") {
  while (endswith(basedir, "/")) {
    basedir = basedir.substr(0, basedir.length() - 1);
  }

  if (basedir.length() == 0) {
    cout << "invalid basedir" << endl;
    exit(1);
  }
  
  this->m_basedir = basedir;
}

FileService::~FileService(){

}

bool FileService::endswith(string str, string suffix) {
  size_t pos = str.rfind(suffix);
  return pos == (str.length() - suffix.length());
}

// Get the contents of a file within our work directory
// Sends it back as an http response (passed in pointer)
void FileService::get(HTTPRequest *request, HTTPResponse *response) {
  string path = this->m_basedir + request->getPath(); // get the file in our work directory
  string fileContents = this->readFile(path); // read file
  if (fileContents.size() == 0) {
    // No file contents
    response->setStatus(403);
    return;
  } else {
    // Has file contents
    // Set the content type appropriately
    if (this->endswith(path, ".css")) {
      response->setContentType("text/css");
    } else if (this->endswith(path, ".js")) {
      response->setContentType("text/javascript");
    }

    // Set the body to content
    response->setBody(fileContents);
  }
}

// read file from path
// returns the contents as a string
string FileService::readFile(string path) {
  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    return "";
  }

  string result;
  int ret;
  char buffer[4096];
  while ((ret = read(fd, buffer, sizeof(buffer))) > 0) {
    result.append(buffer, ret);
  }

  close(fd);
  
  return result;
}

// same as get but no body in response
void FileService::head(HTTPRequest *request, HTTPResponse *response) {
  // HEAD is the same as get but with no body
  this->get(request, response);
  response->setBody("");
}
