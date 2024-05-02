#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <fcntl.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <deque>

#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include "HttpService.h"
#include "HttpUtils.h"
#include "FileService.h"
#include "MySocket.h"
#include "MyServerSocket.h"
#include "dthread.h"

using namespace std;

int PORT = 8080;
int THREAD_POOL_SIZE = 1;
int BUFFER_SIZE = 1;
string BASEDIR = "static";
string SCHEDALG = "FIFO";
string LOGFILE = "/dev/null";

vector<HttpService *> services;

vector<pthread_t *> thread_pool;
deque<MySocket *> buffer;
pthread_mutex_t * lock = new pthread_mutex_t();
pthread_cond_t * dequeue = new pthread_cond_t();
pthread_cond_t * enqueue = new pthread_cond_t();

// find a service that is registered for this path prefix
HttpService *find_service(HTTPRequest *request) {
   // find a service that is registered for this path prefix
  for (unsigned int idx = 0; idx < services.size(); idx++) {
    if (request->getPath().find(services[idx]->pathPrefix()) == 0) {
      return services[idx];
    }
  }

  return NULL;
}

// invoke the service if we found one
void invoke_service_method(HttpService *service, HTTPRequest *request, HTTPResponse *response) {
  stringstream payload;

  // invoke the service if we found one
  if (service == NULL) {
    // not found status
    response->setStatus(404);
  } else if (request->isHead()) {
    service->head(request, response);
  } else if (request->isGet()) {
    service->get(request, response);
  } else {
    // The server doesn't know about this method
    response->setStatus(501);
  }
}

// read in the request
// send data back to the client and clean up
// This is what the main function calls every time we get request from the server
void handle_request(MySocket *client) {
  HTTPRequest *request = new HTTPRequest(client, PORT);
  HTTPResponse *response = new HTTPResponse();
  stringstream payload;
  
  // read in the request
  bool readResult = false;
  try {
    payload << "client: " << (void *) client;
    sync_print("read_request_enter", payload.str());
    readResult = request->readRequest();
    sync_print("read_request_return", payload.str());
  } catch (...) {
    // swallow it
  }    
    
  if (!readResult) {
    // there was a problem reading in the request, bail
    delete response;
    delete request;
    sync_print("read_request_error", payload.str());
    return;
  }
  
  HttpService *service = find_service(request);
  invoke_service_method(service, request, response);

  // send data back to the client and clean up
  payload.str(""); payload.clear();
  payload << " RESPONSE " << response->getStatus() << " client: " << (void *) client;
  sync_print("write_response", payload.str());
  cout << payload.str() << endl;
  client->write(response->response());
    
  delete response;
  delete request;

  payload.str(""); payload.clear();
  payload << " client: " << (void *) client;
  sync_print("close_connection", payload.str());
  client->close();
  delete client;
}

void* start_thread(void * arg) {
  while (true) {
    dthread_mutex_lock(lock);

    while (buffer.empty()) {
      int ret = dthread_cond_wait(enqueue, lock);
      if (ret != 0) {
        cerr << "dthread_cond_wait error number " << ret << endl;
      }
    }

    MySocket* client = buffer.front();
    buffer.pop_front();
    dthread_cond_signal(dequeue);
    dthread_mutex_unlock(lock);

    handle_request(client);
  }
}

int main(int argc, char *argv[]) {

  signal(SIGPIPE, SIG_IGN);
  int option;

  while ((option = getopt(argc, argv, "d:p:t:b:s:l:")) != -1) {
    switch (option) {
    case 'd':
      BASEDIR = string(optarg);
      break;
    case 'p':
      PORT = atoi(optarg);
      break;
    case 't':
      THREAD_POOL_SIZE = atoi(optarg);
      break;
    case 'b':
      BUFFER_SIZE = atoi(optarg);
      break;
    case 's':
      SCHEDALG = string(optarg);
      break;
    case 'l':
      LOGFILE = string(optarg);
      break;
    default:
      cerr<< "usage: " << argv[0] << " [-p port] [-t threads] [-b buffers]" << endl;
      exit(1);
    }
  }

  set_log_file(LOGFILE);

  sync_print("init", "");

  // Create server and client
  MyServerSocket *server = new MyServerSocket(PORT);
  MySocket *client;

  // The order that you push services dictates the search order
  // for path prefix matching
  services.push_back(new FileService(BASEDIR));

  // Add the threads to handle the clients
  thread_pool = vector<pthread_t *>(THREAD_POOL_SIZE);
  for (pthread_t *& thread : thread_pool) {
    thread = new pthread_t();
    int ret = dthread_create(thread, NULL, start_thread, NULL);
    if (ret != 0) {
      cerr << "dthread_create error number " << ret;
      exit(1);
    }

    ret = dthread_detach(*thread);
    if (ret != 0) {
      cerr << "dthread_detach error number " << ret;
      exit(1);
    }
  }
  
  while(true) {
    sync_print("waiting_to_accept", "");
    dthread_mutex_lock(lock);
    while ((int)buffer.size() >= BUFFER_SIZE) {
      dthread_cond_wait(dequeue, lock);
    }
    dthread_mutex_unlock(lock);
    client = server->accept(); // Get client
    sync_print("client_accepted", "");
    dthread_mutex_lock(lock);
    buffer.push_back(client);
    dthread_cond_signal(enqueue);
    dthread_mutex_unlock(lock);
  }
}