#include "sim_interface.h"

std::vector<std::pair<std::string,double> > sim_interface::do_sim(
    std::map<std::string,std::vector<std::pair<docid_t,std::vector<uint32_t> > > > postings,
    std::map<docid_t,std::vector<double> > raw_doc_features,
    std::map<docid_t,std::vector<double> > ranking_features,
    std::vector<std::string> terms,
    int nterms,
    int npairs,
    std::vector<std::vector<uint32_t> > rangeids) {
  std::vector<std::pair<std::string,double> > timing_results;

  // TODO(roasbeef): 
  //   1. Manually intersect r-set ranges?
  //       * func in indexer to do this?
  //   2. Initial fset load
  //        * need to get the total # of (term, doc_id) pairs, tells total 
  //          number of features, then assume uniform distribution across the 
  //          number of shards. (linear regression to fit curve?)
  //        * how to handle the asynchronicity?
  //        * subtract time to do reset of search from time to do fset pre-load
  //      auto initial_fset_preload_time = simulate_fset_preload(num_chunks, f_set_thread_pool_size, num_per_fset_chunk)
  //  3. x-set client
  //      * num_base_terms = nterms
  //      * num_extra_terms = nterms + npairs + npairs
  //      * num_matching_entries = only length of base terms vector? would need to group into chunks?
  //      * include latency also?
  //  4. xset-server
  //      * conjunctive_match_size = same as num_matching_entries ?
  //      * num_xset_threads = std::hardware_concurrency ?
  //      * num_extra_fset_shards = manual range intersection amongst extra terms
  //  5. final_search
  //      * num_matching_base_terms = nterms
  //      * num_matching_extra_terms = nterms + npairs + npairs
  return timing_results;
}

// Time in ms to preload 6,000,000 entries as seen in the trec08 index.
const double PRELOAD_TREC_08 = 8767;

double simulate_fset_preload(int num_chunks, int thread_pool_size, int chunk_size) {
    return (num_chunks * (chunk_size * PRELOAD_TREC_08)) / thread_pool_size;
}

// Time in ms to compute a modular inversion.
const double INVERSION_TIME = 0.034;

// Time in ms to compute an xtoken.
const double XTOKEN_COMPUTE_TIME = 1.4;

double simulate_xset_client(int num_base_terms, int num_matching_entries, 
                            int num_extra_terms) {
    auto t = 0;

    // First, factor in the time it would take to compute the deblinding value 
    // for each of the matching t-set entries for the s-term.
    t += (INVERSION_TIME * num_matching_entries);

    // Next, factor in the time it would take the client to compute the xtokens 
    // for first the base terms, then the extra terms.
    t += (num_matching_entries * (XTOKEN_COMPUTE_TIME * num_base_terms));
    t += (num_matching_entries * (XTOKEN_COMPUTE_TIME * num_extra_terms));

    return t;
}

// Time in ms to compute an xtag, then look it up in a bloom filter with 700 
// million elements.
const double XTAG_LOOKUP_TIME = 1.8;
double simulate_xset_server(int t_set_result_size, int num_xset_threads, int num_terms) {
    double t = 0;

    // To simluate the server's interaction, we compute the amount of time it 
    // would take to generate the xtag, then check the bloom filter for all the 
    // matching t-set entries. This value is multiplied by the number of terms 
    // since each xtoken needs to be checked.
    t += ((t_set_result_size * XTAG_LOOKUP_TIME) * num_terms) / num_xset_threads;

    return t;
}

double simulate_final_search(int num_matching_base_terms, int num_extra_fset_shards, 
                             int num_matching_extra_terms, bool boolean) {
    // (map_lookup_time * num_matching_base_terms)
    //
    // if(boolean) {
    //     ( + rset_intersect_time )
    //     ( + simulate_fset_preload(num_extra_fset_shards, N, CHUNK_SIZE) )
    //     ( + fset_map_lookup_time * num_matching_extra_terms )
    // }
    //
    // (feature_reduce_time * (num_matching_extra_terms + num_matching_base_terms));
    //
    // ( + final processing time)
    return 0;
}
