#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <linux/limits.h>

#include "common.h"

char *malloc_getcwd()
{
  char temp[PATH_MAX];
  char *result = getcwd(temp, sizeof(temp));
  if (result == NULL) {
    errnof("getcwd failed");
    return NULL;
  }
  return strdup(result);
}

void usage()
{
  logf("Usage: inroot <root_dir> <command>...");
  logf();
  logf("Run the given <command> as if <root_dir> is its root directory");
}
int main(int argc, const char *argv[])
{
  argc--;
  argv++;
  if (argc == 0) {
    usage();
    return 1;
  }
  if (argc == 1) {
    errf("please supply a command to run");
    return 1;
  }
  const char *root = argv[0];
  char *cwd = malloc_getcwd();
  if (!cwd)
    return 1; // error already logged
  //logf("[DEBUG] cd '%s'", root);
  if (-1 == chdir(root)) {
    errnof("chdir '%s' failed", root);
    return 1;
  }
  if (-1 == chroot(".")) {
    errnof("chroot '%s' failed", root);
    return 1;
  }
  //logf("[DEBUG] cd '%s'", cwd);
  if (-1 == chdir(cwd)) {
    errnof("chdir '%s' after chroot failed", cwd);
    return 1;
  }
  argc--;
  argv++;
  //for (int i = 0; i < argc; i++) {
  //logf("[%d] '%s'", i, argv[i]);
  //}
  execvp(argv[0], (char *const*)argv);
  errnof("execvp failed");
  return 1;
}
