

#!/bin/bash
index_search=/silo/davidwang/SecureSearch/indexer/index_search
parser=/silo/davidwang/SecureSearch/indexer/parser
echo "Content-type: text/html"
echo
QUERY_STRING="http://localhost/cgi-bin/index_search.sh?query=hello&dataset=aquaint&sort=sort&feature=docid&subbtn=Submit"
#echo "<html><body>"
QUERY=`echo "$QUERY_STRING" | sed -n -e 's/^.*query=\([^&]*\).*$/\1/p' -e "s/%20/ /g"`
DATASET=`echo "$QUERY_STRING" | sed -n -e 's/^.*dataset=\([^&]*\).*$/\1/p' -e "s/%20/ /g"`
SORT=`echo "$QUERY_STRING" | sed -n -e 's/^.*sort=\([^&]*\).*$/\1/p' -e "s/%20/ /g"`
FEATURE=`echo "$QUERY_STRING" | sed -n -e 's/^.*feature=\([^&]*\).*$/\1/p' -e "s/%20/ /g"`
PRE_DATASET=`echo "$QUERY_STRING" | sed -n -e 's/^.*pre_dataset=\([^&]*\).*$/\1/p' -e "s/%20/ /g"`
TREC08_QUERY=`echo "$QUERY_STRING" | sed -n -e 's/^.*trec08_query=\([^&]*\).*$/\1/p' -e "s/%20/ /g"`
TREC45_QUERY=`echo "$QUERY_STRING" | sed -n -e 's/^.*trec45_query=\([^&]*\).*$/\1/p' -e "s/%20/ /g"`
AQUAINT_QUERY=`echo "$QUERY_STRING" | sed -n -e 's/^.*aquaint_query=\([^&]*\).*$/\1/p' -e "s/%20/ /g"`
PREDET=`echo "$QUERY_STRING" | sed -n -e 's/^.*predet=\([^&]*\).*$/\1/p' -e "s/%20/ /g"`
DOCLOCATION=/silo/datasets/indices


if [[ "${FEATURE}" = 'docid' && "${PREDET}" != 'Submit' ]] 
then
    echo $QUERY | "$index_search" --index /silo/datasets/indices/$DATASET/index | "$parser" "$QUERY" "$DATASET" "0"
fi
if [[ "${FEATURE}" != 'docid' && "${PREDET}" != 'Submit' ]]
then
    echo $QUERY | "$index_search" --index /silo/datasets/indices/$DATASET/index --$SORT $FEATURE  | "$parser" "$QUERY" "$DATASET" "0"
fi

if [[ "${PREDET}" = 'Submit' ]]
then
    if [[ "${PRE_DATASET}" = 'aquaint' ]]
    then
	AQUAINT_QUERY1=`echo "$AQUAINT_QUERY" | sed 's/+/ /g'`
	echo $AQUAINT_QUERY1 | "$index_search" --ranking -w "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 3 0 0 0 1 3" --index $DOCLOCATION/$PRE_DATASET/index --qid | /silo/davidwang/SecureSearch/indexer/process_results --numeric-qid --qrels /silo/datasets/ranking_data/qrels.aquant.303-589_50.txt --nfeatures 34 --docids /silo/datasets/titles-aquaint.txt | "$parser" "$AQUAINT_QUERY" "$PRE_DATASET" "1"
    fi
    if [[ "${PRE_DATASET}" = 'trec08' ]]
    then
	TREC08_QUERY1=`echo "$TREC08_QUERY" | sed 's/+/ /g'`
	echo $TREC08_QUERY1 | "$index_search" --ranking -w "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 3 0 0 0 1 3" --index $DOCLOCATION/$PRE_DATASET/index --qid | /silo/davidwang/SecureSearch/indexer/process_results --numeric-qid --qrels /silo/datasets/ranking_data/qrels.08enterprise.CE051-127.txt --nfeatures 34 --docids /silo/datasets/titles-trec08.txt | "$parser" "$TREC_08QUERY" "$PRE_DATASET" "1"
    fi
    if [[ "${PRE_DATASET}" = 'trec45' ]]
    then
	TREC45_QUERY1=`echo "$TREC45_QUERY" | sed 's/+/ /g'`
	echo $TREC45_QUERY1 | "$index_search" --ranking -w "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 3 0 0 0 1 3" --index $DOCLOCATION/$PRE_DATASET/index --qid | /silo/davidwang/SecureSearch/indexer/process_results --numeric-qid --qrels /silo/datasets/ranking_data/qrels.04robust.301-450_601-700.txt --nfeatures 34 --docids /silo/datasets/titles-trec45.txt | "$parser" "$TREC45_QUERY" "$PRE_DATASET" "1"
    fi
fi


#echo "301 International Organized Crime" | ./index_search --ranking -w "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 3 0 0 0 1 3" --index /silo/datasets/indices/trec45/index --qid | ./process_results --numeric-qid --qrels /silo/datasets/ranking_data/qrels.04robust.301-450_601-700.txt --nfeatures 34 --docids /silo/datasets/titles-trec45.txt | less 