/**
 * prep.c
 *
 * Unix utility that recursively searches for matching words in text files,
 * similar to the grep command (specifically with flags -Rnw). Only entire words
 * are matched, not partial words (i.e., searching for 'the' does not match
 * 'theme'). The line number where the matching search term was found is also
 * printed.
 *
 * The tool makes use of multiple threads running in parallel to perform the
 * search. Each file is searched by a separate thread.
 *
 * To build: run `make`.
 *
 * Author: 
 */


#define _GNU_SOURCE
#include <ctype.h> 
#include <dirent.h> 
#include <errno.h> 
#include <getopt.h> 
#include <limits.h> 
#include <pthread.h> 
#include <semaphore.h> 
#include <stdbool.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <unistd.h>

  /* Preprocessor directives */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

# define ANSI_COLOR_CYAN "\x1b[36m"
# define ANSI_COLOR_RESET "\x1b[0m"
# define SEPARATOR "/"

/* Function prototypes */
void print_usage(char * argv[]);

/* Globals */
char * g_search_dir;
bool g_exact = false;
unsigned int g_num_terms = 0;
char ** g_search_terms;
char delimit[] = " \t\r\n.,:?!`()[]-/\'\"<>";

sem_t sem;

/**
 * Prints usage information for the program, generally called when the user
 * passes in the -h flag or an incorrect command line option.
 *
 * Input:
 * - argv: command line arguments passed to the program
 *
 * Returns: N/A
 */
void print_usage(char * argv[]) {
  printf("Usage: %s [-eh] [-d directory] [-t threads] "
    "search_term1 search_term2 ... search_termN\n", argv[0]);
  printf("\n");
  printf("Options:\n"
    "    * -d directory    specify start directory (default: CWD)\n"
    "    * -e              print exact name matches only\n"
    "    * -h              show usage information\n"
    "    * -t threads      set maximum threads (default: # procs)\n");
  printf("\n");
}
/**
 * Retrieves the filename from a file pointer and returns
 * a pointer to the name
 *
 * Input:
 * - FILE to get file name
 *
 * Returns: char pointer to a filename
 */
char * recover_filename(FILE * f) {
  int fd;
  char fd_path[255];
  char * filename = malloc(255);
  ssize_t n;

  fd = fileno(f);
  sprintf(fd_path, "/proc/self/fd/%d", fd);
  n = readlink(fd_path, filename, 255);
  if (n < 0)
    return NULL;
  filename[n] = '\0';
  return filename;
}


/**
 * Searches a file line by line for a string
 * Called on a pthread
 *
 * Input:
 * - the file to process
 *
 * Returns: N/A
 */
void * analyze_file(void * file) {

  int line_num = 0;
  const int line_sz = 100;

  char * ptest = recover_filename(file);

  char * line = malloc(line_sz);
  char * savePtr;
  char * word;
  char match_line[line_sz];

  int found = 0;

  pthread_detach(pthread_self());

  int result = 0;

  while (fgets(line, line_sz, file) != NULL) {

    strcpy(match_line, line);

    word = strtok_r(line, delimit, & savePtr);

    while (word != NULL) {
      int i;
      for (i = 0; i < g_num_terms; i++) {

        if (g_exact)
          result = strcmp(word, g_search_terms[i]);
        else
          result = strcasecmp(word, g_search_terms[i]);

        if (result == 0 && !found) {

          printf("%s:%d:%s\n", ptest, line_num, match_line);
          found = 1;
          break;

        }
      }
      found = 0;
      word = strtok_r(NULL, delimit, & savePtr);

    }

    line_num++;

  }
  free(line);
  sem_post( & sem);
  fclose(file);
  
  return NULL;
}

/**
 * Recursive Directory Listing - traverses directory for all files
 * and subdirectories. If it is called using a file, a pthread
 * is created and the file is searched for the matching terms
 * Input:
 * - path to file/directory
 *
 * Returns: N/A
 */

void ls(const char * path) {
  DIR * directory = opendir(path);

  if (directory != NULL) {
    struct dirent * filei;



    while ((filei = readdir(directory)) != NULL) {
      if ((strcmp(filei->d_name, "..") == 0) || strcmp(filei->d_name, ".") == 0) {
        continue;
      }

      char * buffer = malloc(strlen(path) + strlen(filei->d_name) + strlen(SEPARATOR) + 1);
      strcat(strcat(strcpy(buffer, path), SEPARATOR), filei->d_name);

      struct stat stat_buffer;

      // if file
      if (stat(buffer, & stat_buffer) == 0) {

        FILE * f;
        f = fopen(buffer, "r");
        if (f == NULL) {
          perror(NULL);
        } else {

          pthread_t thread_id;
          sem_wait( & sem);
          pthread_create( & thread_id, NULL, analyze_file, f);

        }

        if (S_ISDIR(stat_buffer.st_mode)) {
          ls(buffer);
        }
      }

      free(buffer);
    }

    closedir(directory);
  } else {
    perror(NULL);
  }
  return;

}

int main(int argc, char * argv[]) {
  

  int num_cores = get_nprocs();
  int max_threads = num_cores;

  int c;
  opterr = 0;
    while ((c = getopt(argc, argv, "d:eht:")) != -1) {
      switch (c) {
      case 'd':
        g_search_dir = optarg;
        break;
      case 'e':
        g_exact = true;
        break;
      case 'h':
        print_usage(argv);
        return 0;
      case 't':
        max_threads = atoi(optarg);
        if (max_threads < 1 || max_threads > num_cores) {
          printf("You entered an invalid number of theads. It must be between 1 and %d", num_cores);
          abort();
        }
        break;
      case '?':
        if (optopt == 'd' || optopt == 't') {
          fprintf(stderr,
            "Option -%c requires an argument.\n", optopt);
        } else if (isprint(optopt)) {
          fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        } else {
          fprintf(stderr,
            "Unknown option character `\\x%x'.\n", optopt);
        }
        return 1;
      default:
        abort();
      }
    }

  sem_init( & sem, 0, max_threads);

  // if no directory is entered, use the current directory
  char cwd[256];
  if (g_search_dir == NULL) {
    getcwd(cwd, sizeof(cwd));
    g_search_dir = cwd;
  }

  g_num_terms = argc - optind;
  g_search_terms = & argv[optind];

  printf("dir is: %s\n", g_search_dir);

  ls(g_search_dir);

  pthread_exit(0);

  return 0;
}