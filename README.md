
 Unix utility that recursively searches for matching words in text files,
 similar to the grep command (specifically with flags -Rnw). Only entire words
 are matched, not partial words (i.e., searching for 'the' does not match
 'theme'). The line number where the matching search term was found is also
 printed.
 
 The tool makes use of multiple threads running in parallel to perform the
 search. Each file is searched by a separate thread.
 
 To build: run `make`.

 Author: David Hutchinson 
 
