#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <mntent.h>

#include <sys/stat.h>
#include <sys/mount.h>

#include "common.h"

char *realpath2(const char *path)
{
  char temp[PATH_MAX];
  char *result = realpath(path, temp);
  return result ? strdup(result) : NULL;
}

err_t loggy_remove(const char *path)
{
  logf("[DEBUG] remove '%s'", path);
  int result = remove(path);
  if (result) {
    errnof("remove '%s' failed", path);
    return 1;
  }
  return 0;
}

unsigned char is_dot_or_dot_dot(const char *s)
{
  return s[0] == '.' &&
    (s[1] == '\0' || (s[1] == '.' && s[2] == '\0'));
}

static unsigned clean_dir(dev_t root_dev, const char *dir, dev_t dir_dev);

static unsigned clean_dir_entries(dev_t root_dev, const char *dir, DIR *dir_handle)
{
  unsigned error_count = 0;
  size_t dir_length = strlen(dir);

  for (;;) {
    errno = 0;
    struct dirent *entry = readdir(dir_handle);
    if (entry == NULL) {
      if (errno) {
        error_count++;
        errnof("readdir '%s' failed", dir);
      }
      break;
    }
    if (is_dot_or_dot_dot(entry->d_name))
      continue;

    size_t entry_name_length = dir_length + 1 + strlen(entry->d_name);
    char *entry_name = malloc(entry_name_length + 1);
    if (!entry_name) {
      errnof("malloc failed");
      continue;
    }
    memcpy(entry_name, dir, dir_length);
    entry_name[dir_length] = '/';
    strcpy(entry_name + dir_length + 1, entry->d_name);

    struct stat entry_stat;
    if (-1 == lstat(entry_name, &entry_stat)) {
      errnof("lstat on '%s' failed", entry_name);
      error_count++;
    } else {
      if (S_ISDIR(entry_stat.st_mode)) {
        error_count += clean_dir(root_dev, entry_name, entry_stat.st_dev);
      } else {
        error_count += loggy_remove(entry_name);
      }
    }
    free(entry_name);
  }
  return error_count;
}

static int loggy_umount(const char *dir)
{
  logf("umount %s", dir);
  if (-1 == umount(dir)) {
    errnof("umount '%s' failed", dir);
    return -1; // fail
  }
  return 0; // success
}

static char *get_biggest_mount(const char *prefix, size_t prefix_length)
{
  char *biggest_mount = NULL;
  size_t biggest_mount_length = 0;

  FILE *mnts = setmntent("/proc/mounts", "r");
  if (mnts == NULL) {
    errnof("setmntent 'proc/mounts' failed");
    return NULL;
  }
  for (;;) {
    struct mntent *entry = getmntent(mnts);
    if (entry == NULL) {
      // TODO: check errno
      break;
    }
    if (0 == strncmp(prefix, entry->mnt_dir, prefix_length)) {
      size_t new_length = strlen(entry->mnt_dir);
      if (new_length > biggest_mount_length) {
        if (biggest_mount)
          free(biggest_mount);
        biggest_mount = strdup(entry->mnt_dir);
        biggest_mount_length = new_length;
      }
    }
  }
  endmntent(mnts);
  return biggest_mount;
}

// TODO: this could be coded to be faster
static unsigned try_clean_mounts(const char *dir)
{
  unsigned unmount_count = 0;
  size_t dir_length = strlen(dir);
  // unmount the biggest ones first, because mounts higher
  // in the tree will not get unmounted if they have submounts
  for (;;) {
    char *biggest_mount = get_biggest_mount(dir, dir_length);
    if (!biggest_mount)
      break;
    int mount_result = loggy_umount(biggest_mount);
    free(biggest_mount);
    if (mount_result == -1)
      break;
    unmount_count++;
  }
  return unmount_count;
}

static unsigned char is_bind_mount(const char *dir, size_t dir_length)
{
  // http://blog.schmorp.de/2016-03-03-detecting-a-mount-point.html
  // This is a hack to determine if a path is a bind mount. It attempts
  // to do an invalid rename which will fail with a different error code
  // depending on if it is a bind mount or not
  char *from = alloca(dir_length + 6);
  char *to = alloca(dir_length + 3);
  memcpy(from, dir, dir_length);
  memcpy(to  , dir, dir_length);
  strcpy(from + dir_length, "/../.");
  strcpy(to   + dir_length, "/.");
  int result = rename(from, to);
  if (result != -1) {
    errf("rename '%s' to '%s' should not have worked!!!!", from, to);
    exit(1);
  }
  return errno == EXDEV;
}

/*
returns: the number of entries it failed to remove
assumption: dir is already verified to be a directory
root_dev is the device number of the directory in the filesystem you
are wanting to clean from
clean_dir uses this to know when a subdirectory is just a mount point to
another filesystem.  it will then proceed to unmount instead of removing
files inside the mount point
*/
static unsigned clean_dir(dev_t root_dev, const char *dir, dev_t dir_dev)
{
  //logf("[DEBUG] clean_dir '%s' (root_dev=%lu, dev=%lu)", dir, root_dev, dir_dev);
  // unmount the directory
  while (dir_dev != root_dev || is_bind_mount(dir, strlen(dir))) {
    // TODO: maybe this loop should have a max number of attempts?

    //loggy.run(["sudo", "umount", dir])
    if (-1 == loggy_umount(dir)) {
      unsigned removed = try_clean_mounts(dir);
      if (removed == 0) {
        // errors already logged
        return 1;
      }
      continue;
    }

    struct stat dir_stat;
    if (-1 == stat(dir, &dir_stat)) {
      errnof("stat on '%s' failed after unmounting it", dir);
      return 1;
    }
    dir_dev = dir_stat.st_dev;
  }

  DIR *dir_handle = opendir(dir);
  if (dir_handle == NULL) {
    errnof("opendir '%s' failed", dir);
    return 1;
  }
  unsigned error_count = clean_dir_entries(root_dev, dir, dir_handle);
  closedir(dir_handle);

  error_count += loggy_remove(dir);
  return error_count;
}

unsigned loggy_rmtree(const char *dir)
{
  logf("[DEBUG] rmtree '%s'", dir);
  struct stat dir_stat;
  if (-1 == stat(dir, &dir_stat)) {
    errnof("stat '%s' failed", dir);
    return 1;
  }
  if (!S_ISDIR(dir_stat.st_mode)) {
    errf("'%s' exists but is not a directory", dir);
    return 1;
  }
  // get the realdir so we can find it's mount points
  const char *realdir = realpath2(dir);
  if (!realdir) {
    errnof("realpath '%s' failed", dir);
    return 1;
  }
  // start by trying to clean up the mounts
  try_clean_mounts(realdir);

  // need to get the new root dev number in case it changed from being unmounted
  if (-1 == stat(realdir, &dir_stat)) {
    errnof("stat '%s' failed", dir);
    return 1;
  }

  return clean_dir(dir_stat.st_dev, realdir, dir_stat.st_dev);
}
