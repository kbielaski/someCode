How to run the gui

1. make sure apache is running
2. make sure /var/www/html contains secure_search.php and /var/www/cgi-bin contains index_search.sh and doc_lookup.sh
3. make sure /silo/daviwang/SecureSearch/indexer contains the executables for index_search, highlight, parser, and doc_lookup
4. go to 127.0.0.1/secure_search.php on your browser to run the gui

How to use
Top Half:
  1. enter your desired search query(ies) into the Search Queries form
  2. determine which dataset you want to look in
  3. pick if you want to sort (from smallest to largest) or rsort(from largest to smallest)
  4. pick which feature you want to sort by (or by docid which is default)
  5. click submit
  
  The results will be displayed with the scores for the various documents, as well as the the document ids/ Once you get to the table holding the results, you can click on the docid to be taken to the document contents. The search queries will be highlighted.

Bottom Half:
  1. pick which dataset that you want to search by
  2. pick the predetermined query that you want to look for in the previously picked dataset (the other two don't matter)
  3. click submit
  
  The results will be given in terms of a cumulative scores that are generated. The table will also contain a "relevant" score of 0 or 1, as well as the scores for all of the features, as well as two document ids. The first 10 results will display the document contents with the queries highlighted. These searches may take a while (about 18-19 seconds), so please be patient.
