#include "index_searcher.h"

typedef uint64_t docid_t;
typedef uint64_t termid_t;

class sim_interface {
  public:
    std::vector<std::pair<std::string,double> > do_sim( 
        std::map<std::string,std::vector<std::pair<docid_t,std::vector<uint32_t> > > > postings, //positional postings
        std::map<docid_t,std::vector<double> > raw_doc_features, //'feature' postings -- value of each doc-term pair (order of vectors is same as in terms)
        std::map<docid_t,std::vector<double> > ranking_features, //fixed list of ranking features (e.g. right now there are 34ish) computed from raw features and some extra info from processing the postings
        std::vector<std::string> terms, //full list of terms, goes: body terms (base terms), title terms, body bigrams, title bigrams -- the counts for each are nterms,nterms,npairs,npairs
        int nterms, //number of base terms
        int npairs, //number of bigrams
        std::vector<std::vector<uint32_t> > rangeids); //range chunk ids for search

  private:
};

/*
 rset+tset workflow:
   * intersect the ranges of base terms
   * start pre-loading fset chunks in the background
   *
   * chunks_searched = 0
   * upper_chunk_limit = s_term_ranges.size() * rset.docs_per_range
   * sterm_index_matches = {}
   * for rset_index in s_term_ranges:
   *     current_pos = rset_index * rset.docs_per_range + 1;  
   *     range_end = current_post + rset.docs_per_range
   *
   *     while(current_pos < range_end):
   *         chunks_search += 1
   *
   *         block_num = (current_pos - 1) / CHUNK_SIZE # t-set chunk size
   *         tset_index = 1 if block_num == 0 else (block_num * CHUNK_SIZE) + 1
   *
   *         record_locator = new_locator(stag, tset_index)
   *         tset_chunk = tset.request_lookup
   *
   *         chunk_pos = (current_pos - 1) % CHUNK_SIZE
   *         chunk_increment = CHUNK_SIZE - chunk_pos
   *
   *         if (current_pos + chunk+pos) > range_end:
   *             current_pos = (range_end - current_pos)
   *             current_pos = chunk_increment
   *
   * send s-term tset results to client
   *
 fset workflow: (4 or N thread operating) takes 70-80ms
    * bytes_read = 0 
    * xtag_to_feature = {}
    * while(bytes_read < file_size):
    *   read (xtag, feature) pair from memory map
    *
    *   xtag_to_feature[xtag] = feature
    *
    *   bytes_read += 41
    * return xtag_to_feature  
  ||
  ||
  \/
 xset client workflow: 
   * t-set results come in
   * for query_terms in [base_terms, extra_terms]:
   *     for chunk in tset-chunks:
   *        for entry in chunk:
   *            if single word query:
   *                collect result continue
   *        compute deblind for this index (z^-1)
   *        for term in terms[1:]:
   *            compute xtokens (g^(xtrap*z^-1))
   *            (send to server?)
  ||
  ||
  \/
 xset server workflow: 
   * --- wait for preload terms to finish ---
   * --- continue after they're loaded    ---
   * matched_tokens = []
   * doc_to_queries = defaultdict(list)
   * for token_desc in x_tokens_desc: (in parallel)
   *   if marked_dead(token_desc): # grabs lock
   *      continue
   *
   *   # Compute xtag. If it doesn't match kill all other pending requests 
   *   for this doc_id, otherwise mark as a match.
   *   blinded_id = token_dec.blinded_id
   *   xtag = x_token^blinded_id
   *   if is_in_bloom_filter(xtag):
   *      matched_tokens.append(token_desc)
   *   else:
   *      mark extra terms related to doc_id dead # grabs lock
   *
   * return matched_tokens
  ||
  ||
  \/
 final search workflow: 
  *  matching_docs = set()
  *  matching_doc_features = {} # doc_id -> (query_type -> (term, feature))
  * 
  *  # Collect all features for base matching docs, matching extra terms separelty
  *  for token_desc in matched_tokens: # NOTE: the kill signals actually happen here
  *      if token_desc.query_type == Base:
  *          if (!xset.lookup_feature(token_desc.xtag)):
  *             continue # feature not found on disk
             matching_doc_features[desc.doc_id][token_dec.query_type].append((term, feature))
  *      else:
  *          extra_terms_to_doc_id[query_term].push_back((doc_id, xtag))
  *      matching_docs.append(doc_id)
  *
  * # If doing a boolean search, retrieve the remaining feature shards. Note 
  * # that this de-duplicates work by skipping any f-set shards already loaded into memory.
  * if conjunctive_query:
  *     # Load up the features for the matching extra terms
  *     # This should actually happen back on the client
  *     final_matches = [sterm] + extra_terms_to_doc_id.keys()
  *     ranges = rset.intersect_ranges(final_matches)
  *     fset_shards = preload_shards(ranges)
  * 
  *     for extra_match extra_terms_to_doc_id:
  *         for xtag in extra_match.xtag_pairs:
  *             if (!xset.lookup_feature(xtags):
  *                 continue
  *         matching_doc_features[doc_id][query_type].push_back((term, f))
  * 
  * # Reduce the encrypted features to their final form
  * reducer = init_reducer(ranking_algo_type)
  * doc_to_final_features = reduce_features_by_category(matching_doc_features, reducer)
  *
  * # Sum up all the features across each category for each doc_d
  * ranking_results = {} # doc_id -> float
  * for doc_result in doc_to_final_features:
  *     doc_id = doc_result.id
  *     feature_groups = doc_result.second
  *
  *     for feature_group in feature_groups:
  *         for feature in feature_group
  *             ranking_results[doc_id] += feature
  *
  * === search fin ===
 */
