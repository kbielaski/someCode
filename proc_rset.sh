sed -e 's/^query(qid:\([^)]*\)) raw( \(.*\) ) clean( \([^)]*\) )/qid\t\1\nraw\t\2\nclean\t\3/;s/R-store T-store saved chunks: \([0-9]*\) \/ \([0-9]*\) .*/tstore\t\1\t\2/;s/R-store X-store saved chunks: \([0-9]*\) \/ \([0-9]*\) .*/xstore\t\1\t\2/;s/R-store total saved chunks: \([0-9]*\) \/ \([0-9]*\) .*/total\t\1\t\2/;s/Search Result Count: /totalresults\t/;/\t/!d' | \
awk '
/^qid\t/{ q=$2; r="";c="";t1="";x1="";t2="";x2="";s1="";s2=""; tr=""; }
/^raw\t/{ sub("raw\t",""); r=$0; }
/^clean\t/{ sub("clean\t",""); c=$0; }
/^tstore\t/{ t1=$2;t2=$3; }
/^xstore\t/{ x1=$2;x2=$3; }
/^totalresults\t/{ tr=$2; }
/^total\t/{ s1=$2;s2=$3;  printf("%s\t%s\t%s\t\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",q,r,c,t1,t2,x1,x2,s1,s2,tr); }
'
