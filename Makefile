.PHONY:all
all: binary_index_gen bigram_gen index_scan index_search index_stats doc_lookup process_results

index_search1: index_search1.cc index_searcher.cc serializer.cc stemmer.cc index_searcher.h serializer.h stemmer.h tfidf.h bm25.h helpers.h
	g++ --std=c++11 -o index_search1 index_search1.cc index_searcher.cc serializer.cc stemmer.cc -lssl -lcrypto

index_search: index_search.cc index_searcher.cc serializer.cc stemmer.cc index_searcher.h serializer.h stemmer.h tfidf.h bm25.h helpers.h
	g++ --std=c++11 -o index_search index_search.cc index_searcher.cc serializer.cc stemmer.cc -lssl -lcrypto

index_stats: index_stats.cc index_searcher.cc serializer.cc stemmer.cc index_scanner.cc index_searcher.h index_scanner.h serializer.h stemmer.h helpers.h
	g++ --std=c++11 -o index_stats index_stats.cc index_searcher.cc index_scanner.cc serializer.cc stemmer.cc -lssl -lcrypto

doc_lookup: doc_lookup.cc index_searcher.cc serializer.cc stemmer.cc index_searcher.h serializer.h stemmer.h helpers.h
	g++ --std=c++11 -o doc_lookup doc_lookup.cc index_searcher.cc serializer.cc stemmer.cc -lssl -lcrypto

index_read: index_read.cc coding.cc coding.h helpers.h
	g++ --std=c++11 -o index_read index_read.cc coding.cc -lssl -lcrypto

binary_index_gen: binary_index_gen.cc serializer.cc LineScanner.cc stemmer.cc CorpusScanner.h DocumentScanner.h LineScanner.h bigram_gen.h binary_index_gen.h serializer.h stemmer.h coding.h coding.cc helpers.h
	g++ -o binary_index_gen binary_index_gen.cc serializer.cc LineScanner.cc stemmer.cc coding.cc -lssl -lcrypto

bigram_gen: bigram_gen.cc serializer.cc LineScanner.cc stemmer.cc CorpusScanner.h DocumentScanner.h LineScanner.h bigram_gen.h serializer.h stemmer.h helpers.h
	g++ -o bigram_gen bigram_gen.cc serializer.cc LineScanner.cc stemmer.cc -lssl -lcrypto

quick_search: quick_search.cc index_searcher.cc coding.cc coding.h index_searcher.h stemmer.h helpers.h
	g++ --std=c++11 -o quick_search quick_search.cc index_searcher.cc coding.cc -lssl -lcrypto

index_scan: index_scan.cc serializer.cc index_searcher.cc index_scanner.cc serializer.h index_searcher.h index_scanner.h helpers.h
	g++ --std=c++11 -o index_scan index_scan.cc serializer.cc index_searcher.cc index_scanner.cc -lssl -lcrypto

dictionary_scan: dictionary_scan.cc serializer.cc serializer.h helpers.h
	g++ --std=c++11 -o dictionary_scan dictionary_scan.cc serializer.cc -lssl -lcrypto

process_results: process_results.cc helpers.h
	g++ -o process_results process_results.cc

parser: parser.cpp helpers.h
	g++ --std=c++11 -o parser parser.cpp

highlight: highlight.cpp stemmer.h index_searcher.h stemmer.cc index_searcher.cc serializer.h serializer.cc helpers.h
	g++ --std=c++11 -o highlight highlight.cpp stemmer.cc index_searcher.cc serializer.cc -lssl -lcrypto

stem: stem.cc stemmer.h stemmer.cc helpers.h
	g++ -o stem stem.cc stemmer.cc

.PHONY:clean
clean:
	rm -f index_search1 binary_index_gen bigram_gen index_scan index_search index_stats doc_lookup
