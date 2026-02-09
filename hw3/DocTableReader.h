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

#ifndef HW3_DOCTABLEREADER_H_
#define HW3_DOCTABLEREADER_H_

#include <string>    // for string
#include <cstdio>    // for (FILE*)

#include "./HashTableReader.h"

namespace hw3 {

// A DocTableReader reads the docid->docname table within an index file.
class DocTableReader : public HashTableReader {
 public:
  // Construct a new DocTableReader at a specified offset within
  // an index file.  Arguments:
  //
  // - f: an open (FILE*) for the underlying index file.  The
  //   constructed  object takes ownership of the (FILE*) and will
  //   fclose() it on destruction.
  //
  // - offset: the "doctable"'s byte offset within the file.
  DocTableReader(FILE* f, IndexFileOffset_t offset);
  ~DocTableReader() { }

  // Lookup a docID and get back a string containing the filename
  // associated with the docID, if it exists.
  //
  // Arguments:
  // - docid: the docID to look for within the doctable.
  // - ret_str: the string containing the filename (an output parameter).
  //   Nothing is returned through this if the docID is not found.
  //
  // Returns:
  // - true if the docID is found, false otherwise.
  bool LookupDocID(const DocID_t &doc_id, std::string* ret_str) const;

 private:
  // This friend declaration is here so that the Test_DocTableReader
  // unit test fixture can access protected member variables of
  // DocTableReader.
  friend class Test_DocTableReader;

  DISALLOW_COPY_AND_ASSIGN(DocTableReader);
};

}  // namespace hw3

#endif  // HW3_DOCTABLEREADER_H_
