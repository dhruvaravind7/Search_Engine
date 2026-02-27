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

#ifndef HW4_HTTPREQUEST_H_
#define HW4_HTTPREQUEST_H_

#include <map>
#include <string>

namespace hw4 {

// This class represents an HTTP Request. For our website search engine, we
// will only handle "GET"-style requests, meaning the request will have the
// following format:
//
// GET [URI] [http_protocol]\r\n
// [headername]: [headerval]\r\n
// [headername]: [headerval]\r\n
// ... more headers ...
// [headername]: [headerval]\r\n
// \r\n
//
// e.g.:
//
// GET /foo/bar?baz=bam HTTP/1.1\r\n
// Host: www.news.com\r\n
//
class HttpRequest {
 public:
  HttpRequest() { }
  explicit HttpRequest(const std::string &uri) : uri_(uri) { }
  virtual ~HttpRequest() { }

  const std::string& uri() const { return uri_; }
  void set_uri(const std::string& uri) { uri_ = uri; }

  // Returns the value associated with the passed-in header name, or empty
  // string if it does not exist in the header map.
  std::string GetHeaderValue(const std::string &name) const {
    std::string n = tolower_copy(name);
    std::map<std::string, std::string>::const_iterator it = headers_.find(n);
    if (it == headers_.end()) {
      return "";
    } else {
      return it->second;
    }
  }

  // Adds a name -> value mapping to the header map, over-writing any existing
  // previous mapping for name.
  void AddHeader(const std::string &name, const std::string &value) {
    std::string n = tolower_copy(name);
    headers_[n] = value;
  }

  // Returns the number of headers this HttpRequest contains
  int GetHeaderCount() {
    return headers_.size();
  }

 private:
  // Which URI did the client request?
  std::string uri_;

  // A map from mapping a header name to a header value, which represents the
  // headers a client would supply to us. Due to RFC 2616:4.2 stating that
  // header names are case-insensitive, we internally convert all header names
  // to lowercase.  But note that the header values can remain case-sensitive.
  std::map<std::string, std::string> headers_;

  static std::string tolower_copy(const std::string &s) {
    std::string ret;
    for (const auto c : s) {
      ret.push_back(std::tolower(c));
    }
    return ret;
  }
};

}  // namespace hw4

#endif  // HW4_HTTPREQUEST_H_
