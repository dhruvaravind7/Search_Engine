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

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <sstream>

#include "./FileReader.h"
#include "./HttpConnection.h"
#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpServer.h"
#include "./libhw3/QueryProcessor.h"

using std::cerr;
using std::cout;
using std::endl;
using std::list;
using std::map;
using std::string;
using std::stringstream;
using std::unique_ptr;

namespace hw4 {
///////////////////////////////////////////////////////////////////////////////
// Constants, internal helper functions
///////////////////////////////////////////////////////////////////////////////
// static
const int HttpServer::kNumThreads = 8;

static const char* kThreegleStr =
"<html><head><title>333gle</title></head>\n"
"<body>\n"
"<center style=\"font-size:500%;\">\n"
"<span style=\"position:relative;bottom:-0.33em;color:orange;\">3</span>"
"<span style=\"color:red;\">3</span>"
"<span style=\"color:gold;\">3</span>"
"<span style=\"color:blue;\">g</span>"
"<span style=\"color:green;\">l</span>"
"<span style=\"color:red;\">e</span>\n"
"</center>\n"
"<p>\n"
"<div style=\"height:20px;\"></div>\n"
"<center>\n"
"<form action=\"/query\" method=\"get\">\n"
"<input type=\"text\" size=30 name=\"terms\" />\n"
"<input type=\"submit\" value=\"Search\" />\n"
"</form>\n"
"</center><p>\n";

// This is the function that threads are dispatched into
// in order to process new client connections.
static void HttpServer_ThrFn(ThreadPool::Task* t);

// Given a request, produce a response.
static HttpResponse ProcessRequest(const HttpRequest &req,
                                   const string &base_dir,
                                   const list<string> &indices);

// Process a file request.
static HttpResponse ProcessFileRequest(const string &uri,
                                       const string &base_dir);

// Process a query request.
static HttpResponse ProcessQueryRequest(const string &uri,
                                        const list<string> &indices,
                                        const string &base_dir);

// Returns true if 's' starts with 'prefix'.
static bool StringStartsWith(const string &s, const string &prefix);

///////////////////////////////////////////////////////////////////////////////
// HttpServer
///////////////////////////////////////////////////////////////////////////////
bool HttpServer::Run(void) {
  // Create the server listening socket.
  int listen_fd;
  cout << "  creating and binding the listening socket..." << endl;
  if (!socket_.BindAndListen(AF_INET6, &listen_fd)) {
    cerr << endl << "Couldn't bind to the listening socket." << endl;
    return false;
  }

  // Spin, accepting connections and dispatching them.  Use a
  // threadpool to dispatch connections into their own thread.
  tp_.reset(new ThreadPool(kNumThreads));
  cout << "  accepting connections..." << endl << endl;
  while (!IsShuttingDown()) {
    // If the HST is successfully added to the threadpool, it'll (eventually)
    // get run and clean itself up.  But we need to manually delete it if
    // it doesn't get added.
    HttpServerTask* hst = new HttpServerTask(HttpServer_ThrFn, this);
    hst->base_dir = static_file_dir_path_;
    hst->indices = &indices_;

    if (!socket_.Accept(&hst->client_fd,
                        &hst->c_addr,
                        &hst->c_port,
                        &hst->c_dns,
                        &hst->s_addr,
                        &hst->s_dns)) {
      // The accept failed for some reason, so quit out of the server.  This
      // can happen when the `kill` command is used to shut down the server
      // instead of the more graceful /quitquitquit handler.
      delete hst;
      break;
    }

    // The accept succeeded; dispatch it to the workers.
    if (!tp_->Dispatch(hst)) {
      delete hst;
      break;
    }
  }
  return true;
}

void HttpServer::BeginShutdown() {
  Verify333(pthread_mutex_lock(&lock_) == 0);
  shutting_down_ = true;
  tp_->BeginShutdown();
  Verify333(pthread_mutex_unlock(&lock_) == 0);
}

bool HttpServer::IsShuttingDown() {
  bool retval;
  Verify333(pthread_mutex_lock(&lock_) == 0);
  retval = shutting_down_;
  Verify333(pthread_mutex_unlock(&lock_) == 0);
  return retval;
}

///////////////////////////////////////////////////////////////////////////////
// Internal helper functions
///////////////////////////////////////////////////////////////////////////////
static void HttpServer_ThrFn(ThreadPool::Task* t) {
  // Cast back our HttpServerTask structure with all of our new client's
  // information in it.  Since we the ones that created this object, we are
  // guaranteed that this is an instance of a HttpServerTask and, per Google's
  // Style Guide, can use a static_cast<> instead of a dynamic_cast<>.
  //
  // Note that, per the ThreadPool::Task API, it is the job of this function
  // to clean up the dynamically-allocated task object.
  unique_ptr<HttpServerTask> hst(static_cast<HttpServerTask*>(t));
  cout << "  client " << hst->c_dns << ":" << hst->c_port << " "
       << "(IP address " << hst->c_addr << ")" << " connected." << endl;

  // Read in the next request, process it, and write the response.

  // Use the HttpConnection class to read and process the next HTTP request
  // from our current client, then write out our response.  Recall that
  // multiple HTTP requests can be sent on the same TCP connection; we
  // need to keep the connection alive until the client sends a
  // "Connection: close\r\n" header; it is only after we finish processing
  // their request that we can shut down the connection and exit
  // this function.

  // STEP 1:
  HttpConnection hc(hst->client_fd);
  HttpRequest rq;  // you should probably initialize this somehow
  while (!hst->server_->IsShuttingDown()) {
    if (!hc.GetNextRequest(&rq)) {
      break;
    }
    // If the client requested the server to shut down, do so.
    if (StringStartsWith(rq.uri(), "/quitquitquit")) {
      hst->server_->BeginShutdown();
      break;
    }
    HttpResponse resp = ProcessRequest(rq, hst->base_dir, *(hst->indices));
    if (!hc.WriteResponse(resp)) {
      break;
    }
    if (rq.GetHeaderValue("connection") == "close") {
      break;
    }
  }
}

static HttpResponse ProcessRequest(const HttpRequest &req,
                                   const string &base_dir,
                                   const list<string> &indices) {
  // Is the user asking for a static file?
  if (StringStartsWith(req.uri(), "/static/")) {
    return ProcessFileRequest(req.uri(), base_dir);
  }

  // The user must be asking for a query.
  return ProcessQueryRequest(req.uri(), indices, base_dir);
}

static HttpResponse ProcessFileRequest(const string &uri,
                                       const string &base_dir) {
  // The response we'll build up.
  HttpResponse ret;

  // Steps to follow:
  // 1. Use the URLParser class to figure out what file name
  //    the user is asking for. Note that we identify a request
  //    as a file request if the URI starts with '/static/'
  //
  // 2. Use the FileReader class to read the file into memory
  //
  // 3. Copy the file content into the ret.body
  //
  // 4. Depending on the file name suffix, set the response
  //    Content-type header as appropriate, e.g.,:
  //      --> for ".html" or ".htm", set to "text/html"
  //      --> for ".jpeg" or ".jpg", set to "image/jpeg"
  //      --> for ".png", set to "image/png"
  //      etc.
  //    You should support the file types mentioned above,
  //    as well as ".txt", ".js", ".css", ".xml", ".gif",
  //    and any other extensions to get bikeapalooza
  //    to match the solution server.
  //
  // be sure to set the response code, protocol, and message
  // in the HttpResponse as well.
  string file_name = "";

  // STEP 2:
  URLParser parser;
  parser.Parse(uri);

  string path = parser.path();
  file_name = path.substr(string("/static/").size());
  if (!IsPathSafe(base_dir, base_dir + "/" + file_name)) {
    ret.set_protocol("HTTP/1.1");
    ret.set_response_code(403);
    ret.set_message("Forbidden");
    ret.AppendToBody("<html><body>Forbidden</body></html>\n");
    return ret;
  }
  FileReader fr(base_dir, file_name);

  string contents;
  if (fr.ReadFile(&contents)) {
    ret.AppendToBody(contents);
    ret.set_protocol("HTTP/1.1");
    ret.set_response_code(200);
    ret.set_message("OK");

    string ext = "";
    size_t dot_pos = file_name.find_last_of(".");
    if (dot_pos != string::npos) {
      ext = file_name.substr(dot_pos);
    }
    if (ext == ".html" || ext == ".htm") ret.set_content_type("text/html");
    else if (ext == ".jpeg" || ext == ".jpg")
    ret.set_content_type("image/jpeg");
    else if (ext == ".png") ret.set_content_type("image/png");
    else if (ext == ".txt") ret.set_content_type("text/plain");
    else if (ext == ".js") ret.set_content_type("application/javascript");
    else if (ext == ".css") ret.set_content_type("text/css");
    else if (ext == ".xml") ret.set_content_type("application/xml");
    else if (ext == ".gif") ret.set_content_type("image/gif");
    else ret.set_content_type("text/plain");

    return ret;
  }
  // If you couldn't find the file, return an HTTP 404 error.
  ret.set_protocol("HTTP/1.1");
  ret.set_response_code(404);
  ret.set_message("Not Found");
  ret.AppendToBody("<html><body>Couldn't find file \""
                   + EscapeHtml(file_name)
                   + "\"</body></html>\n");
  return ret;
}

static HttpResponse ProcessQueryRequest(const string &uri,
                                        const list<string> &indices,
                                        const string &base_dir) {
  HttpResponse ret;
  ret.set_protocol("HTTP/1.1");
  ret.set_response_code(200);
  ret.set_message("OK");
  ret.set_content_type("text/html");

  // STEP 3:
  // Parse the URI to extract query args
  URLParser parser;
  parser.Parse(uri);
  auto args = parser.args();

  // Build the 333gle search page header
  ret.AppendToBody(kThreegleStr);

  // If there are search terms, process the query
  if (args.find("terms") != args.end()) {
    string terms = args["terms"];

    // Convert terms to lowercase
    transform(terms.begin(), terms.end(), terms.begin(), ::tolower);

    // Split the terms into a vector of words
    std::vector<string> query_words;
    std::istringstream iss(terms);
    string word;
    while (iss >> word) {
      query_words.push_back(word);
    }

    // Use QueryProcessor to process the query
    hw3::QueryProcessor qp(indices);
    std::vector<hw3::QueryProcessor::QueryResult> results =
    qp.ProcessQuery(query_words);

    // Display results
    ret.AppendToBody("<p><b>Results for \"" + EscapeHtml(terms) +
    "\":</b></p>\n");
    if (results.empty()) {
      ret.AppendToBody("<p>No results found.</p>\n");
    } else {
      ret.AppendToBody("<ul>\n");
      // Normalize base_dir to ensure it has a trailing slash
      char real_base[PATH_MAX];
      char real_doc[PATH_MAX];
      realpath(base_dir.c_str(), real_base);
      string base_str(real_base);
      if (base_str.back() != '/') base_str += '/';

      for (const auto &result : results) {
        string doc_name = result.document_name;
        if (doc_name.substr(0, 4) == "http") {
          ret.AppendToBody("<li><a href=\"" + doc_name + "\">"
                          + EscapeHtml(doc_name) + "</a> ["
                          + std::to_string(result.rank) + "]</li>\n");
          continue;
        } else if (realpath(doc_name.c_str(), real_doc) != nullptr) {
          string real_doc_str(real_doc);
          if (real_doc_str.find(base_str) == 0) {
            doc_name = real_doc_str.substr(base_str.length());
          }
        }

        ret.AppendToBody("<li><a href=\"/static/" + doc_name + "\">"
                        + EscapeHtml(doc_name) + "</a> ["
                        + std::to_string(result.rank) + "]</li>\n");
      }
      ret.AppendToBody("</ul>\n");
    }
  }

  ret.AppendToBody("</body></html>\n");
  return ret;
}

static bool StringStartsWith(const string &s, const string &prefix) {
  return s.substr(0, prefix.size()) == prefix;
}


}  // namespace hw4
