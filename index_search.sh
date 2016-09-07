#!/bin/bash
index_search=/silo/davidwang/SecureSearch/indexer/index_search
parser=/silo/davidwang/SecureSearch/indexer/parser
echo "Content-type: text/html"
echo
#echo "<html><body>"
QUERY=`echo "$QUERY_STRING" | sed -n -e 's/^.*query=\([^&]*\).*$/\1/p' -e "s/%20/ /g"`
DATASET=`echo "$QUERY_STRING" | sed -n -e 's/^.*dataset=\([^&]*\).*$/\1/p' -e "s/%20/ /g"`
SORT=`echo "$QUERY_STRING" | sed -n -e 's/^.*sort=\([^&]*\).*$/\1/p' -e "s/%20/ /g"`
FEATURE=`echo "$QUERY_STRING" | sed -n -e 's/^.*feature=\([^&]*\).*$/\1/p' -e "s/%20/ /g"`
##"$index_search" --index /silo/datasets/indices/trec08/index
##"$index_search" --index /silo/datasets/indices/trec08/index <<< "apple train" | "$parser"
echo $QUERY | "$index_search" --index /silo/datasets/indices/$DATASET/index --$SORT $FEATURE  | "$parser" "$QUERY" "$DATASET"
#echo PARAMS | "$index_search" --index /silo/datasets/indices/trec08/index | parser.cpp
#echo "</html></body>"