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

#include "./HashTableReader.h"

#include <stdint.h>  // for uint32_t, etc.
#include <cstdio>    // for (FILE*).
#include <list>      // for std::list.

#include "./LayoutStructs.h"

extern "C" {
  #include "libhw1/CSE333.h"
}
#include "./Utils.h"  // for FileDup().


using std::list;

namespace hw3 {

HashTableReader::HashTableReader(FILE* f, IndexFileOffset_t offset)
  : file_(f), offset_(offset) {
  // STEP 1.
  // fread() the bucket list header in this hashtable from its
  // "num_buckets" field, and convert to host byte order.
  fseek(file_, offset_, SEEK_SET);
  Verify333(fread(&header_, sizeof(BucketListHeader), 1, file_) == 1);
  header_.ToHostFormat();
}

HashTableReader::~HashTableReader() {
  fclose(file_);
  file_ = nullptr;
}

list<IndexFileOffset_t>
HashTableReader::LookupElementPositions(HTKey_t hash_key) const {
  // Figure out which bucket the hash value is in.  We assume
  // hash values are mapped to buckets using the modulo (%) operator.
  int bucket_num = hash_key % header_.num_buckets;

  // Figure out the offset of the "bucket_rec" field for this bucket.
  IndexFileOffset_t bucket_rec_offset =
      offset_ + sizeof(BucketListHeader) + sizeof(BucketRecord) * bucket_num;

  // STEP 2.
  // Read the "chain len" and "bucket position" fields from the
  // bucket record, and convert from network to host order.
  BucketRecord bucket_rec;
  fseek(file_, bucket_rec_offset, SEEK_SET);
  int32_t chain_len;
  IndexFileOffset_t bucket_pos;
  fread(&chain_len, sizeof(int32_t), 1, file_);
  fread(&bucket_pos, sizeof(IndexFileOffset_t), 1, file_);
  bucket_rec = BucketRecord(chain_len, bucket_pos);
  bucket_rec.ToHostFormat();

  // This will be our returned list of element positions.
  list<IndexFileOffset_t> ret_val;

  // STEP 3.
  // Read the "element positions" fields from the "bucket" header into
  // the returned list.  Be sure to insert into the list in the
  // correct order (i.e., append to the end of the list).
  for (int i = 0; i < bucket_rec.chain_num_elements; i++) {
    ElementPositionRecord element_pos_rec;
    fseek(file_, bucket_rec.position + sizeof(ElementPositionRecord) * i
    , SEEK_SET);
    fread(&element_pos_rec.position, sizeof(IndexFileOffset_t), 1, file_);
    element_pos_rec.ToHostFormat();
    ret_val.push_back(element_pos_rec.position);
  }


  // Return the list.
  return ret_val;
}
}  // namespace hw3
