#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "common.h"

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
  {
    int old_argc = argc;
    argc = 0;
    int arg_index = 0;
    for (; arg_index < old_argc; arg_index++) {
      const char *arg = argv[arg_index];
      if (arg[0] != '-') {
        argv[argc++] = arg;
      } else {
        errf("unknown option '%s'", arg);
        return 1;
      }
    }
  }
  if (argc == 0) {
    usage();
    return 1;
  }
  if (argc == 1) {
    errf("please supply a command to run");
    return 1;
  }
  const char *root = argv[0];
  if (-1 == chroot(root)) {
    errnof("chroot '%s' failed", root);
    return 1;
  }
  argc--;
  argv++;
  execvp(argv[0], (char *const*)argv);
  errnof("execvp failed");
  return 1;
}
