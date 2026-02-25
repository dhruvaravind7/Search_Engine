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

#include "./QueryProcessor.h"

#include <iostream>
#include <algorithm>
#include <list>
#include <string>
#include <vector>

extern "C" {
  #include "./libhw1/CSE333.h"
}

using std::list;
using std::sort;
using std::string;
using std::vector;

namespace hw3 {

QueryProcessor::QueryProcessor(const list<string> &index_list, bool validate) {
  // Stash away a copy of the index list.
  index_list_ = index_list;
  array_len_ = index_list_.size();
  Verify333(array_len_ > 0);

  // Create the arrays of DocTableReader*'s. and IndexTableReader*'s.
  dtr_array_ = new DocTableReader*[array_len_];
  itr_array_ = new IndexTableReader*[array_len_];

  // Populate the arrays with heap-allocated DocTableReader and
  // IndexTableReader object instances.
  list<string>::const_iterator idx_iterator = index_list_.begin();
  for (int i = 0; i < array_len_; i++) {
    FileIndexReader fir(*idx_iterator, validate);
    dtr_array_[i] = fir.NewDocTableReader();
    itr_array_[i] = fir.NewIndexTableReader();
    idx_iterator++;
  }
}

QueryProcessor::~QueryProcessor() {
  // Delete the heap-allocated DocTableReader and IndexTableReader
  // object instances.
  Verify333(dtr_array_ != nullptr);
  Verify333(itr_array_ != nullptr);
  for (int i = 0; i < array_len_; i++) {
    delete dtr_array_[i];
    delete itr_array_[i];
  }

  // Delete the arrays of DocTableReader*'s and IndexTableReader*'s.
  delete[] dtr_array_;
  delete[] itr_array_;
  dtr_array_ = nullptr;
  itr_array_ = nullptr;
}

// This structure is used to store a index-file-specific query result.
typedef struct {
  DocID_t doc_id;  // The document ID within the index file.
  int rank;        // The rank of the result so far.
} IdxQueryResult;

std::vector<QueryProcessor::QueryResult>
QueryProcessor::ProcessQuery(const std::vector<std::string> &query) const {
  Verify333(query.size() > 0);

  // STEP 1.
  // (the only step in this file)

  std::vector<QueryResult> final_result;
  for (int i = 0; i < array_len_; i++) {
    std::vector<DocID_t> docids;
    std::vector<int> ranks;
    bool first_word = true;
    bool dead = false;

    for (const std::string &term : query) {
      DocIDTableReader *ditr = itr_array_[i]->LookupWord(term);
      if (ditr == nullptr) {
        dead = true;
        break;
      }

      std::list<DocIDElementHeader> docs_for_term = ditr->GetDocIDList();
      delete ditr;
      if (first_word) {
        for (const auto &hdr : docs_for_term) {
          docids.push_back(hdr.doc_id);
          ranks.push_back(hdr.num_positions);
        }
        first_word = false;
        if (docids.empty()) {
          dead = true;
          break;
        }
      } else {
        std::vector<DocID_t> new_docids;
        std::vector<int> new_ranks;
        for (size_t k = 0; k < docids.size(); k++) {
          DocID_t id = docids[k];
          int curr_rank = ranks[k];
          bool found = false;
          int occ = 0;
          for (const auto &hdr : docs_for_term) {
            if (hdr.doc_id == id) {
              found = true;
              occ = hdr.num_positions;
              break;
            }
          }
          if (found) {
            new_docids.push_back(id);
            new_ranks.push_back(curr_rank + occ);
          }
        }
        docids.swap(new_docids);
        ranks.swap(new_ranks);
        if (docids.empty()) {
          dead = true;
          break;
        }
      }
    }
    if (dead || first_word) {
      continue;
    }
    for (size_t k = 0; k < docids.size(); k++) {
      std::string name;
      Verify333(dtr_array_[i]->LookupDocID(docids[k], &name));
      QueryResult qr;
      qr.document_name = name;
      qr.rank = ranks[k];
      final_result.push_back(qr);
    }
  }

  sort(final_result.begin(), final_result.end());
  return final_result;
}
}  // namespace hw3
