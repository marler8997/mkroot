#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>

#include <sys/stat.h>
#include <sys/mount.h>

#include <linux/limits.h>

#include "common.h"
#include "vector.h"
#include "concat.h"

const char *get_opt_arg(int argc, const char *argv[], int *arg_index)
{
  (*arg_index)++;
  if (*arg_index >= argc) {
    errf("option '%s' requires an argument", argv[(*arg_index) - 1]);
    exit(1);
  }
  return argv[*arg_index];
}

unsigned get_dir_length(const char *file)
{
  const char * s = strrchr(file, '/');
  if (s == NULL) {
    return 0;
  }
  return s - file;
}

char *realpath2(const char *path)
{
  char temp[PATH_MAX];
  char *result = realpath(path, temp);
  return result ? strdup(result) : NULL;
}

const char *lstrip(const char *str, char strip_char)
{
  for (; str[0] == strip_char; str++) { }
  return str;
}
const char *rstrip(const char *str, char strip_char)
{
  size_t length = strlen(str);
  if (length == 0)
    return str;
  length--;
  if (str[length] != strip_char)
    return str;
  for (; length > 0 && str[length-1] == strip_char; length--)
    { }
  char *new_str = malloc(length + 1);
  memcpy(new_str, str, length);
  new_str[length] = '\0';
  return new_str;
}

enum string_compare_result {
  // neither string starts with the other
  STRING_COMPARE_DISJOINT = 0,
  STRING_COMPARE_EQUAL = 1,
  STRING_COMPARE_RIGHT_STARTS_WITH_LEFT = 2,
  STRING_COMPARE_LEFT_STARTS_WITH_RIGHT = 3,
};
enum string_compare_result compare_strings(const char *left, const char *right)
{
  for (;;) {
    char leftc = left[0];
    char rightc = right[0];
    if (leftc == '\0')
      return (rightc == '\0') ? STRING_COMPARE_EQUAL : STRING_COMPARE_RIGHT_STARTS_WITH_LEFT;
    if (leftc != rightc)
      return (rightc == '\0') ? STRING_COMPARE_LEFT_STARTS_WITH_RIGHT : STRING_COMPARE_DISJOINT;
    left++;
    right++;
  }
}

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

enum dir_status {
  DIR_STATUS_DOES_NOT_EXIST = 0,
  DIR_STATUS_NOT_A_DIR = 1,
  DIR_STATUS_EMPTY = 2,
  DIR_STATUS_NOT_EMPTY = 3,
};
// returns: -1 on fail, 0 on success
err_t get_dir_status(enum dir_status *status, const char *dir)
{
  {
    struct stat dir_stat;
    if (-1 == stat(dir, &dir_stat)) {
      if (errno != ENOENT) {
        errnof("stat '%s' failed", dir);
        return err_fail;
      }
      *status = DIR_STATUS_DOES_NOT_EXIST;
      return err_pass;
    }
    if (!S_ISDIR(dir_stat.st_mode)) {
      *status = DIR_STATUS_NOT_A_DIR;
      return err_fail;
    }
  }

  DIR *dir_handle = opendir(dir);
  if (dir_handle == NULL) {
    errnof("opendir '%s' failed", dir);
    return err_fail;
  }

  for (;;) {
    errno = 0;
    struct dirent *entry = readdir(dir_handle);
    if (entry == NULL) {
      if (errno) {
        errnof("readdir '%s' failed", dir);
        closedir(dir_handle);
        return err_fail;
      }
      *status = DIR_STATUS_EMPTY;
      break;
    }
    if (0 == strcmp(".", entry->d_name) || 0 == strcmp("..", entry->d_name))
      continue;
    *status = DIR_STATUS_NOT_EMPTY;
    break;
  }
  closedir(dir_handle);
  return err_pass;
}

static int loggy_mkdir(const char *dir, mode_t mode)
{
  logf("mkdir -m %o %s", mode, dir);
  if (-1 == mkdir(dir, mode)) {
    errnof("mkdir '%s' failed", dir);
    return -1;
  }
  return 0;
}

static int loggy_mount(const char *source, const char *target,
                const char *filesystemtype, const char *options)
{
  logf("mount%s%s%s%s %s %s",
       filesystemtype ? " -t " : "",
       filesystemtype ? filesystemtype : "",
       options ? " -o " : "", options ? options : "",
       source ? source : "\"\"", target);
  if (-1 == mount(source, target, filesystemtype, 0, options)) {
    errnof("mount failed");
    return -1; // fail
  }
  return 0; // success
}

static int loggy_bind_mount(const char *source, const char *target)
{
  logf("mount --bind %s %s", source, target);
  if (-1 == mount(source, target, NULL, MS_BIND, NULL)) {
    errnof("bind mount failed");
    return -1; // fail
  }
  return 0; // success
}

#define DEFAULT_MKDIR_MODE S_IRWXU | S_IRWXG| S_IROTH | S_IXOTH

// returns: 0 on success
err_t mkdirs_helper(char *dir, size_t length)
{
  if (dir[length] != '\0') {
    errf("code bug: mkdirs was called with a string that did not end in null");
    return 1;
  }
  //logf("[DEBUG] mkdirs '%s'", dir);
  {
    struct stat dir_stat;
    if (0 == stat(dir, &dir_stat)) {
      if (S_ISDIR(dir_stat.st_mode)) {
        return 0; // success
      }
      errf("'%s' exists but is not a directory", dir);
      return 1;
    }
  }
  {
    unsigned parent_dir_length = get_dir_length(dir);
    if (parent_dir_length == length) {
      errf("failed to create directory '%s'", dir);
      return 1;
    }
    if (parent_dir_length > 0) {
      dir[parent_dir_length] = '\0';
      int result = mkdirs_helper(dir, parent_dir_length);
      dir[parent_dir_length] = '/';
      if (result)
        return result;
    }
  }
  if (-1 == loggy_mkdir(dir, DEFAULT_MKDIR_MODE)) {
    // error already logged
    return current_error;
  }
  return 0; // success
}
err_t mkdirs(char *dir)
{
  return mkdirs_helper(dir, strlen(dir));
}

struct dir
{
  const char *arg;
  const char *source;
  // if given, this directory is to be writeable, and should be the
  // upper directory of an overlay
  const char *workdir;
};
DEFINE_TYPED_VECTOR(dir, struct dir);

struct mount_point;
DEFINE_TYPED_VECTOR(mount_point, struct mount_point);

const unsigned char MOUNT_POINT_CAN_MKDIRS = 0x01;

struct mount_point
{
  struct dir_vector dirs;
  struct mount_point_vector sub_mount_points;
  const char *target_relative;
  char *private_target_absolute;
  unsigned char flags;
};
err_t mount_point_init(struct mount_point *mount_point, struct dir *first_dir, const char *target_relative)
{
  memset(mount_point, 0, sizeof(*mount_point));
  if (dir_vector_alloc(&mount_point->dirs, 8))
    return err_fail;
  if (dir_vector_add(&mount_point->dirs, first_dir)) {
    dir_vector_free(&mount_point->dirs);
    return err_fail;
  }
  if (mount_point_vector_alloc(&mount_point->sub_mount_points, 8)) {
    dir_vector_free(&mount_point->dirs);
    return err_fail;
  }
  mount_point->target_relative = target_relative;
  return err_pass;
}
void mount_point_free_members(struct mount_point *mount_point)
{
  free(mount_point->private_target_absolute);
  mount_point_vector_free(&mount_point->sub_mount_points);
  dir_vector_free(&mount_point->dirs);
}
struct mount_point *mount_point_alloc(struct dir *first_dir, const char *target_relative)
{
  struct mount_point *mount_point = malloc(sizeof(struct mount_point));
  if (!mount_point)
    goto err;
  if (mount_point_init(mount_point, first_dir, target_relative))
    goto err;
  return mount_point;
 err:
  errnof("could not allocate mount point for '%s'", first_dir->arg);
  mount_point_free_members(mount_point);
  free(mount_point);
  return NULL;
}

static struct dir view_dir;
//static const char *view_path; // the root directory we will chroot to
//static size_t view_path_length;

char *get_absolute_target(struct mount_point *mount_point)
{
  if (!mount_point->private_target_absolute) {
    //const char *separator = (view_path_length == 0 || view_path[view_path_length - 1] == '/') ? "" : "/";
    //mount_point->private_target_absolute = concat(view_path, separator, mount_point->target_relative);
    mount_point->private_target_absolute = concat(view_dir.source, "/", mount_point->target_relative);
  }
  return mount_point->private_target_absolute;
}

static void print_tab(unsigned depth)
{
  // TODO: very inneficient
  for (unsigned i = 0; i < depth; i++)
    printf(" ");
}
static void print_mount_points(struct mount_point_vector *mount_points, unsigned depth);
static void print_mount_point(struct mount_point *mount_point, unsigned depth)
{
  print_tab(depth);logf("target /%s", mount_point->target_relative);
  for (size_t i = 0; i < dir_vector_size(&mount_point->dirs); i++) {
    struct dir *dir = dir_vector_get(&mount_point->dirs, i);
    print_tab(depth);logf("source %s", dir->source);
  }
  print_mount_points(&mount_point->sub_mount_points, depth + 1);
}
static void print_mount_points(struct mount_point_vector *mount_points, unsigned depth)
{
  for (size_t i = 0; i < mount_point_vector_size(mount_points); i++) {
    struct mount_point *mount_point = mount_point_vector_get(mount_points, i);
    print_mount_point(mount_point, depth);
  }
}

err_t add_new_mount_point(struct mount_point_vector *mount_points, struct dir *first_dir, const char *target_relative)
{
  struct mount_point *mount_point = mount_point_alloc(first_dir, target_relative);
  if (!mount_point)
    goto err;
  if (mount_point_vector_add(mount_points, mount_point))
    goto err;
  return err_pass;
 err:
  errnof("failed to add '%s' to mount point list", first_dir->arg);
  mount_point_free_members(mount_point);
  free(mount_point);
  return err_fail;
}

err_t add_dir(struct mount_point_vector *mount_points, struct dir *dir, const char *target_relative)
{
  for (size_t i = 0; i < mount_point_vector_size(mount_points); i++) {
    struct mount_point *mount_point = mount_point_vector_get(mount_points, i);
    enum string_compare_result compare_result = compare_strings(target_relative,
                                                                mount_point->target_relative);
    switch (compare_result) {
    case STRING_COMPARE_DISJOINT:
      break;
    case STRING_COMPARE_EQUAL:
      if (dir_vector_add(&mount_point->dirs, dir)) {
        errnof("failed to add dir '%s' to mount_point dirs vector", dir->arg);
        return err_fail;
      }
      return err_pass;
    case STRING_COMPARE_RIGHT_STARTS_WITH_LEFT:
      // swap the mount points
      {
        struct mount_point *new_mount_point;
        new_mount_point = mount_point_alloc(dir, target_relative);
        if (!new_mount_point)
          return err_fail;
        mount_point_vector_set(mount_points, i, new_mount_point);
        if (mount_point_vector_add(&new_mount_point->sub_mount_points, mount_point))
          return err_fail;
      }
      return err_pass;
    case STRING_COMPARE_LEFT_STARTS_WITH_RIGHT:
      return add_dir(&mount_point->sub_mount_points, dir, target_relative);
    }
  }
  return add_new_mount_point(mount_points, dir, target_relative);
}

// returns: 0 on error, 1 if no mount parent, otherwise, the pointer to the parent mount directory
struct dir *get_mount_parent_for(struct mount_point *mount_point, struct mount_point *sub_mount_point)
{
  const char *target_diff = lstrip(sub_mount_point->target_relative +
                                   strlen(mount_point->target_relative), '/');
  //logf("[DEBUG] target '%s' sub-mount target '%s' diff '%s'",
  //     mount_point->target_relative,
  //     sub_mount_point->target_relative,
  //     target_diff);
  for (int i = 0; i < dir_vector_size(&mount_point->dirs); i++) {
    struct dir *dir = dir_vector_get(&mount_point->dirs, i);
    // check if dir has a directory to accomodate
    char *subdir = concat(dir->source, "/", target_diff);
    if (!subdir) {
      errnof("concat paths failed");
      return 0; // error
    }
    struct stat subdir_stat;
    if (-1 == stat(subdir, &subdir_stat)) {
      if (errno != ENOENT) {
        errnof("stat '%s' failed", subdir);
        free(subdir);
        return 0; // error
      }
    } else {
      if (S_ISDIR(subdir_stat.st_mode)) {
        free(subdir);
        // TODO: should we check all directories to find conflicts?
        //       maybe not since the first directory with the match would
        //       probably take precedence in the overlay
        return dir; // success, have parent dir
      }
      errf("invalid mounts: '%s' is not a directory", subdir);
      free(subdir);
      return 0; // error
    }
    free(subdir);
  }
  return (struct dir*)1; // no parent mount
}

static err_t mount_overlay(const char *target_dir, struct mount_point *mount_point, unsigned char mount_point_has_files)
{
  if (mount_point_has_files) {
    errf("not impl mount point has files");
    return err_fail;
  }

  //
  // get the size of all the directories and also,
  // see if one of the directories has a 'workdir' meaning it should be the upper
  //
  const size_t LOWERDIR_PREFIX_SIZE =  9; // "lowerdir="
  const size_t UPPERDIR_PREFIX_SIZE = 10; // ",upperdir="
  const size_t WORKDIR_PREFIX_SIZE  =  9; // ",workdir="

  struct dir *upper_dir = NULL;
  size_t upper_dir_size = 0;

  size_t lower_dirs_size = 0;
  for (size_t i = 0; i < dir_vector_size(&mount_point->dirs); i++) {
    struct dir *dir = dir_vector_get(&mount_point->dirs, i);
    if (dir->workdir) {
      if (upper_dir) {
        errf("mount point at '%s' has multiple upper directories '%s' and '%s'",
             target_dir, upper_dir->arg, dir->arg);
        return err_fail;
      }
      upper_dir = dir;
      // ",upperdir=%s,workdir=%s"
      upper_dir_size =
        UPPERDIR_PREFIX_SIZE + // ",upperdir="
        strlen(dir->source) + // <upperdir>
        WORKDIR_PREFIX_SIZE + // ",workdir="
        strlen(dir->workdir); // <workdir>
    } else {
      if (lower_dirs_size > 0)
        lower_dirs_size++;
      lower_dirs_size += strlen(dir->source);
    }
  }

  // lowerdir=<lower_dirs>
  size_t options_size = LOWERDIR_PREFIX_SIZE + lower_dirs_size + upper_dir_size;
  char *options = malloc(options_size + 1);
  if (!options) {
    errnof("malloc failed");
    return err_fail;
  }

  // TODO: do not add the rootfs as a lowerdir if there are no non-root mounts
  size_t offset = 0;
  memcpy(options + offset, "lowerdir=", LOWERDIR_PREFIX_SIZE);
  offset += LOWERDIR_PREFIX_SIZE;
  for (size_t i = 0; i < dir_vector_size(&mount_point->dirs); i++) {
    struct dir *dir = dir_vector_get(&mount_point->dirs, i);
    if (dir->workdir)
      continue;
    if (offset > LOWERDIR_PREFIX_SIZE)
      options[offset++] = ':';
    lower_dirs_size += strlen(dir->source);
    size_t len = strlen(dir->source);
    memcpy(options + offset, dir->source, len);
    offset += len;
  }
  if (upper_dir) {
    memcpy(options + offset, ",upperdir=", UPPERDIR_PREFIX_SIZE);
    offset += UPPERDIR_PREFIX_SIZE;
    {
      size_t len = strlen(upper_dir->source);
      memcpy(options + offset, upper_dir->source, len);
      offset += len;
    }
    memcpy(options + offset, ",workdir=", WORKDIR_PREFIX_SIZE);
    offset += WORKDIR_PREFIX_SIZE;
    {
      size_t len = strlen(upper_dir->workdir);
      memcpy(options + offset, upper_dir->workdir, len);
      offset += len;
    }
  }
  if (offset != options_size) {
    errf("code bug: options_size %lu != offset %lu", options_size, offset);
    return err_fail;
  }
  options[offset] = '\0';
  //logf("[DEBUG] overlay options = '%s'", options);
  if (-1 == loggy_mount("none", target_dir, "overlay", options)) {
    // error already logged
    free(options);
    return err_fail;
  }
  free(options);
  return err_pass;
}

static err_t prepare_sub_mounts_helper(struct mount_point *mount_point,
                                       struct mount_point_vector *need_dirs)
{
  // check if we need to make any directories for sub mount points
  for (size_t i = 0; i < mount_point_vector_size(&mount_point->sub_mount_points); i++) {
    struct mount_point *sub_mount_point = mount_point_vector_get(&mount_point->sub_mount_points, i);
    struct dir *mount_parent = get_mount_parent_for(mount_point, sub_mount_point);
    if (mount_parent == 0)
      return err_fail;
    if (mount_parent == (struct dir*)1) {
      if (mount_point_vector_add(need_dirs, sub_mount_point))
        return err_fail;
    } else {
      logf("[DEBUG] mount parent for '%s' is '%s'", sub_mount_point->target_relative, mount_parent->source);
    }
  }

  if (mount_point_vector_size(need_dirs) == 0)
    return err_pass;

  // the mount_point is not writeable and there's no directory to hold one or more
  // sub mounts. in this case we will overlay the mount point with a tmpfs that contains
  // the directories we need for the sub mounts

  /*
  // add the tmpfs to the dirs
  // NOTE: this will not be freed, that's OK
  struct dir *tmpfs_dir = malloc(sizeof(struct dir));
  tmpfs_dir.arg = target_dir;
  tmpfs_dir.source = target_dir;
  */

  // TODO: there is a corner case here where this mount_point is actually going to
  //       be used as it's own sub mount point, in which case mounting over it here would mask out
  //       the files we were trying to mount. I'm not 100% sure this can happen but I should see if I
  //       can write a test for it.

  // this will not be freed, that's ok
  struct dir *tmpfs = malloc(sizeof(struct dir));
  if (!tmpfs) {
    errnof("malloc failed");
    return err_fail;
  }

  // TODO: I'm not sure that we should tmpfs onto the current mount point.
  const char *mount_target_dir = get_absolute_target(mount_point);
  if (!mount_target_dir)
    return err_fail;
  // this will not be freed, that's ok
  tmpfs->arg = strdup(mount_target_dir);
  if (!tmpfs->arg) {
    errnof("strdup failed");
    return err_fail;
  }
  // this should already be a valid source since it's built from
  // an absolute target dir which is built from the view_dir source concatenated
  // with the mount_points relative target dir
  tmpfs->source = tmpfs->arg;
  if (-1 == loggy_mount("tmpfs", tmpfs->source, "tmpfs", NULL)) {
    errnof("failed to mount tmpfs to '%s', it was needed to create mount points", tmpfs->source);
    return err_fail;
  }

  for (size_t i = 0; i < mount_point_vector_size(need_dirs); i++) {
    struct mount_point *sub_mount_point = mount_point_vector_get(need_dirs, i);
    char *target_dir = get_absolute_target(sub_mount_point);
    if (mkdirs(target_dir))
      return err_fail;
  }

  // TODO: we may need to add tmpfs to the front so it
  //       overwrites any other lower directories that may have
  //       this path as a file or something.  But if we have a conflict
  //       like this we may just consider it an invalid view.
  if (dir_vector_add(&mount_point->dirs, tmpfs))
    return err_fail;

  return err_pass;
}

static err_t prepare_sub_mounts(struct mount_point *mount_point)
{
  if (mount_point->flags & MOUNT_POINT_CAN_MKDIRS) {
    for (size_t i = 0; i < mount_point_vector_size(&mount_point->sub_mount_points); i++) {
      struct mount_point *sub_mount_point = mount_point_vector_get(&mount_point->sub_mount_points, i);
      char *target_dir = get_absolute_target(sub_mount_point);
      if (!target_dir)
        return err_fail;
      if (mkdirs(target_dir))
        return err_fail;
    }
    return err_pass;
  }

  struct mount_point_vector need_dirs;
  memset(&need_dirs, 0, sizeof(need_dirs));
  err_t result = prepare_sub_mounts_helper(mount_point, &need_dirs);
  mount_point_vector_free(&need_dirs);
  return result;
}

static err_t make_mount_point(struct mount_point *mount_point);

static err_t make_sub_mount_points(struct mount_point *mount_point)
{
  for (size_t i = 0; i < mount_point_vector_size(&mount_point->sub_mount_points); i++) {
    struct mount_point *sub_mount_point = mount_point_vector_get(&mount_point->sub_mount_points, i);
    err_t result = make_mount_point(sub_mount_point);
    if (result)
      return err_fail;
  }
  return err_pass;
}

static err_t make_mount_point(struct mount_point *mount_point)
{
  if (prepare_sub_mounts(mount_point))
    return err_fail;

  const char *target_dir = get_absolute_target(mount_point);
  if (target_dir == NULL)
    return err_fail; // error already printed

  if (dir_vector_size(&mount_point->dirs) > 1) {
    if (mount_overlay(target_dir, mount_point, 0))
      return err_fail;
  } else  {
    struct dir *dir = dir_vector_get(&mount_point->dirs, 0);
    if (-1 == loggy_bind_mount(dir->source, target_dir)) {
      // error already printed
      return err_fail;
    }
    // remount it as readonly?
    // see https://lwn.net/Articles/281157/
    // it looks like if you want bind mounts to be readonly, you need to mount
    // them as writeable first, and then remount them as readonly
    // mount -o remount,ro <mount_point>
#if 0
    if (!have_upper || i > 0) {
      if (-1 == loggy_mount(NULL, target_dir, NULL, "remount,ro")) {
        // error already logged
        free(target_dir);
        return err_fail;
      }
    }
#endif
  }
  return make_sub_mount_points(mount_point);
}

// verify root directory either does not exist, or is empty
err_t init_root_dir()
{
 enum dir_status status;
 if (get_dir_status(&status, view_dir.source))
   return err_fail;

 switch (status) {
 case DIR_STATUS_DOES_NOT_EXIST:
   return loggy_mkdir(view_dir.source, DEFAULT_MKDIR_MODE) ? err_fail : err_pass;
 case DIR_STATUS_NOT_A_DIR:
   errf("root dir '%s' is not a directory", view_dir.source);
   return err_fail;
 case DIR_STATUS_EMPTY:
   return err_pass;
 case DIR_STATUS_NOT_EMPTY:
   errf("root directory '%s' is not empty", view_dir.source);
   return err_fail;
 }
 errf("codebug: path should not be taken");
 return err_fail;
}

err_t verify_custom_target(const char *target)
{
  if (target[0] == '/') {
    errf("invalid target '%s', cannot begin with '/'", target);
    return err_fail;
  }
  // TODO: verify target does not contain double slashes or '.' directories or '..' directories
  return err_pass;
}

void usage()
{
  logf("Usage: mkview [-options] <view_dir> <dirs>...");
  logf();
  logf("Create a 'root-filesystem view' with the given <dir>s. The view is made up of various");
  logf("bind and overlay mounts. The view can be cleaned up using 'rmr <view_dir>' without");
  logf("removing files from the source directories.");
  logf();
  logf("Each directory is of the form:");
  logf("  [<workdir>,]<dir>[:<target_path>]...");
  logf();
  logf("If a <workdir> is given, then the directory will be writeable and will be the upper");
  logf("directory if it is part of an overlay with other directories. <target_path> is the path");
  logf("where this directory should be exposed on the resulting view.  If it is not given, it will");
  logf("default to the path where the directory existing on the current filesystem.  This path should");
  logf("NOT contain a leading slash '/', an empty path will result having the directory part of the");
  logf("root directory of the new view.");
}

static struct mount_point root_mount_point;
err_t main(int argc, const char *argv[])
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
    errf("please provide one or more directories to include");
    return 1;
  }

  memset(&view_dir, 0, sizeof(view_dir));
  view_dir.arg = rstrip((char*)argv[0], '/');
  view_dir.source = view_dir.arg;

  // make sure that root directory either does not exist or is empty
  {
    err_t result = init_root_dir();
    if (result)
      return result;
  }

  const char **dir_args = argv + 1;
  int dir_count = argc - 1;

  struct dir *dirs = (struct dir*)malloc(sizeof(struct dir) * dir_count);
  if (!dirs) {
    errnof("malloc failed");
    return 1;
  }
  memset(dirs, 0, sizeof(dirs[0]) * dir_count);

  if (mount_point_init(&root_mount_point, &view_dir, ""))
    return err_fail;
  root_mount_point.flags |= MOUNT_POINT_CAN_MKDIRS;

  for (int dir_index = 0; dir_index < dir_count; dir_index++) {
    struct dir *dir = &dirs[dir_index];
    dir->arg = dir_args[dir_index];

    const char *source = dir->arg;
    {
      const char *comma_str = strchr(source, ',');
      if (comma_str) {
        dir->workdir = strndup(source, comma_str - source);
        if (!dir->workdir) {
          errnof("strndup failed");
          return 1;
        }
        source = comma_str + 1;
      }
    }
    const char *target_relative = NULL;
    {
      const char *colon_str = strchr(source, ':');
      if (colon_str) {
        target_relative = colon_str + 1;
        if (verify_custom_target(target_relative)) {
          return err_fail; // error already loggeed
        }
        source = strndup(source, colon_str - source);
        if (!source) {
          errnof("strndup failed");
          return 1;
        }
      }
    }
    struct stat path_stat;
    if (-1 == stat(source, &path_stat)) {
      errnof("'%s'", source);
      return current_error;
    }
    dir->source = realpath2(source);
    if (dir->source == NULL) {
      errnof("realpath('%s') failed", source);
      return current_error;
    }
    if (target_relative == NULL)
      target_relative = lstrip(dir->source, '/');
    logf("source '%s' target '%s'", dir->source, target_relative);
    if (add_dir(&root_mount_point.sub_mount_points, dir, target_relative))
      return err_fail; // error already logged
  }

  // print the mount tree
  logf("--------------------------------------------------------------------------------");
  logf("MOUNT TREE");
  logf("--------------------------------------------------------------------------------");
  print_mount_points(&root_mount_point.sub_mount_points, 0);
  logf("--------------------------------------------------------------------------------");

  if (prepare_sub_mounts(&root_mount_point))
    return err_fail;
  return make_sub_mount_points(&root_mount_point);
}
