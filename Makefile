prep: prep.c
	gcc -g -lpthread -Wall -std=c99 prep.c -o prep

clean:
	rm -f prep
