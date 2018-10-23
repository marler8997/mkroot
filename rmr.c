#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

#include <sys/stat.h>

#include "common.h"
#include "clean.h"

void usage()
{
  logf("Usage: rmr <dir>...");
  logf();
  logf("Unmounts and removes all directories/files in <dir>");
}

int main(int argc, char *argv[])
{
  argc--;
  argv++;
  if (argc == 0) {
    usage();
    return 1;
  }

  unsigned error_count = 0;
  for (int i = 0; i < argc; i++) {
    const char *dir = argv[i];
    struct stat dir_stat;
    if (-1 == stat(dir, &dir_stat)) {
      if (errno == ENOENT) {
        return 0;
      }
      errnof("stat '%s' failed", dir);
      return 1;
    }
    error_count += loggy_rmtree(dir);
  }

  if (error_count == 0)
    logf("\nSuccess");
  else
    logf("\n%d Errors", error_count);
  return error_count;
}
