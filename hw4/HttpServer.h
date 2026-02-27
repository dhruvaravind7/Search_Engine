/*
 * Copyright ©2026 Justin Hsia and Amber Hu. All rights reserved.
 * Permission is hereby granted to students registered for University of
 * Washington CSE 333 for use solely during Winter Quarter 2026 for
 * purposes of the course. No other use, copying, distribution, or
 * modification is permitted without prior written consent. Copyrights
 * for third-party components of this work must be honored. Instructors
 * interested in reusing these course materials should contact the
 * authors.
 */

#ifndef HW4_HTTPSERVER_H_
#define HW4_HTTPSERVER_H_

#include <cstdint>
#include <list>
#include <memory>
extern "C" {
#include <pthread.h>  // for the pthread threading/mutex functions
}
#include <string>

#include "./ThreadPool.h"
#include "./ServerSocket.h"

namespace hw4 {

// The HttpServer class contains the main logic for the web server.
class HttpServer {
 public:
  // Creates a new HttpServer object for port "port" and serving
  // files out of path "static_file_dir_path".  The indices for
  // query processing are located in the "indices" list.
  explicit HttpServer(uint16_t port,
                      const std::string &static_file_dir_path,
                      const std::list<std::string> &indices)
    : socket_(port),
      static_file_dir_path_(static_file_dir_path),
      indices_(indices),
      shutting_down_(false) {
    Verify333(pthread_mutex_init(&lock_, nullptr) == 0);
  }

  // The destructor closes the listening socket if it is open and
  // also terminates any threads in the threadpool.
  virtual ~HttpServer() { }

  // Creates a listening socket for the server and launches it, accepting
  // connections and dispatching them to worker threads.
  //
  // Returns: true if the server was able to start and run; false otherwise.
  //
  // The server will continue to run in this method until a kill command is
  // used to send a SIGTERM signal to the server process (i.e., kill pid,
  // ctrl+C) or until a special handler -- /quitquitquit -- has been requested
  // from the server. The latter is handy when the server is run under
  // Valgrind, which intercepts the kill command and prevents accurate
  // heap statistics from being finalized.
  bool Run();

  // Tell the HttpServer to begin shutting down.  This will cause the server
  // to stop accepting new requests and also to issue a BeginShutdown()
  // to its contained threadpool.  Eventually, once all currently inflight
  // work has completed, the server will finish execution of its Run()
  // method.  Finishing in-flight work but not accepting new work is
  // sometimes known as "lame duck mode".
  void BeginShutdown();

  // Has the server initiated its shutdown?
  bool IsShuttingDown();

 private:
  // Only accessed from the "main thread".
  ServerSocket socket_;
  std::string static_file_dir_path_;

  // Accessed by multiple threads, all read-only.
  std::list<std::string> indices_;

  // May be mutated from multiple threads.  Protected by lock_.
  pthread_mutex_t lock_;
  std::unique_ptr<ThreadPool> tp_;
  bool shutting_down_;

  static const int kNumThreads;
};

class HttpServerTask : public ThreadPool::Task {
 public:
  HttpServerTask(ThreadPool::thread_task_fn f, HttpServer* s)
    : ThreadPool::Task(f), server_(s) { }

  ~HttpServerTask() { }

  int client_fd;
  uint16_t c_port;
  std::string c_addr, c_dns, s_addr, s_dns;
  std::string base_dir;
  std::list<std::string>* indices;
  HttpServer* server_;
};

}  // namespace hw4

#endif  // HW4_HTTPSERVER_H_
