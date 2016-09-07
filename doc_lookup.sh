#!/bin/bash
doc_lookup=/home/kellybielaski/data/SecureSearch/indexer/doc_lookup
parser=/home/kellybielaski/data/SecureSearch/indexer/parser
highlight=/home/kellybielaski/data/SecureSearch/indexer/highlight
#parser=/silo/davidwang/SecureSearch/indexer/parser
#highlight=/silo/davidwang/SecureSearch/indexer/highlight
#doc_lookup=/silo/davidwang/SecureSearch/indexer/doc_lookup
echo "Content-type: text/html"
echo
echo "<html><body>"
QUERY_STRING="http://localhost/cgi-bin/doc_lookup.sh?query=apple+train&dataset=aquaint&docid=14756"
QUERY=`echo "$QUERY_STRING" | sed -n -e 's/^.*query=\([^&]*\).*$/\1/p' -e "s/%20/ /g"`
DOCID=`echo "$QUERY_STRING" | sed -n -e 's/^.*docid=\([^&]*\).*$/\1/p' -e "s/%20/ /g"`
DATASET=`echo "$QUERY_STRING" | sed -n -e 's/^.*dataset=\([^&]*\).*$/\1/p' -e "s/%20/ /g"`
highlight=/silo/davidwang/SecureSearch/indexer/highlight
##"$index_search" --index /silo/datasets/indices/trec08/index
##"$index_search" --index /silo/datasets/indices/trec08/index <<< "apple train" | "$parser"
#echo /silo/datasets/indices/$DATASET/index
#echo /silo/datasets/lines-$DATASET.txt
"$doc_lookup" --index /silo/datasets/indices/$DATASET/index --corpus /silo/datasets/lines-$DATASET.txt $DOCID | "$highlight" "$QUERY"

#"$doc_lookup" --index /silo/datasets/indices/$DATASET/index --corpus /silo/datasets/lines-$DATASET.txt $DOCID | "$highlight" "$QUERY"
#echo PARAMS | "$index_search" --index /silo/datasets/indices/trec08/index | parser.cpp
echo "</html></body>"