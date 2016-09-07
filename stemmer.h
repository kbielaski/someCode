#ifndef _STEMMER_H_
#define _STEMMER_H_

/* This is the Porter stemming algorithm, coded up as thread-safe ANSI C
   by the author.

   It may be be regarded as cononical, in that it follows the algorithm
   presented in

   Porter, 1980, An algorithm for suffix stripping, Program, Vol. 14,
   no. 3, pp 130-137,

   only differing from it at the points maked --DEPARTURE-- below.

   See also http://www.tartarus.org/~martin/PorterStemmer

   The algorithm as described in the paper could be exactly replicated
   by adjusting the points of DEPARTURE, but this is barely necessary,
   because (a) the points of DEPARTURE are definitely improvements, and
   (b) no encoding of the Porter stemmer I have seen is anything like
   as exact as this version, even with the points of DEPARTURE!

   You can compile it on Unix with 'gcc -O3 -o stem stem.c' after which
   'stem' takes a list of inputs and sends the stemmed equivalent to
   stdout.

   The algorithm as encoded here is particularly fast.

   Release 1: the basic non-thread safe version

   Release 2: this thread-safe version

   Release 3: 11 Apr 2013, fixes the bug noted by Matt Patenaude (see the
       basic version for details)

   Release 4: 25 Mar 2014, fixes the bug noted by Klemens Baum (see the
       basic version for details)


   Notes (17 Feb 2015) - Michael : Extracted key code so you can call the function on individual strings
*/

#include <stdlib.h>  /* for malloc, free */
#include <string.h>  /* for memcmp, memmove */
#include <ctype.h>  /* for tolower */

/* You will probably want to move the following declarations to a central
   header file.
*/

struct stemmer;

extern struct stemmer * create_stemmer(void);
extern void free_stemmer(struct stemmer * z);

extern int stem(struct stemmer * z, char * b, int k);
extern char *stem_str(struct stemmer *z, char *b);



/* The main part of the stemming algorithm starts here.
*/

#define TRUE 1
#define FALSE 0

/* stemmer is a structure for a few local bits of data,
*/

struct stemmer {
   char * b;       /* buffer for word to be stemmed */
   int k;          /* offset to the end of the string */
   int j;          /* a general offset into the string */
};

#endif
