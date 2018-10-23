
#if 0
static err_t doit(struct dir *dirs, int dir_count)
{
  /*
    TODO: I need to sort all the mount points and mount them
          from the top down.  Every time I perform a mount, I need
          to check if that mount has all the directories it needs to
          do the next mounts.  If it doesn't, then it needs to be mounted
          alongside a tmpfs overlay that creates the directory for the
          child mounts.
    Example:
          a/
          b/baz
          mkview view a: b:bin
          ---
          so in this case, a has no bin directory but b is going
          to be mounted there.  so we need to mount a as an overlay
          mkdir -p view/bin
          mount -t overlay none view -o lowerdir=a,view
          mount --bind b view/bin
   */

  unsigned non_root_mounts = 0;

  // make the mount point directories
  for (int i = 0; i < dir_count; i++) {
    struct dir *dir = &dirs[i];
    if (is_root_mount(dir))
      continue;

    non_root_mounts++;
    char *target_dir = dir_get_absolute_target(dir);
    if (target_dir == NULL) {
      // error already printed
      return 1;
    }
    {
      err_t result = mkdirs(target_dir);
      if (result)
        return result;
    }
  }

  // create the root mount overlay (do this before
  // mounting anything inside this directory)
  if (non_root_mounts < dir_count)
  {
    // make sure root directory exists
    // it's possible it was not created if there were no
    // non-root mount points
    {
      // todo: maybe don't use strdup
      err_t result = mkdirs(strdup(root));
      if (result)
        return result;
    }

    size_t lower_dirs_size = 0;
    for (int i = 0; i < dir_count; i++) {
      // TODO: handle upper dir properly
      struct dir *dir = &dirs[i];
      if (!is_root_mount(dir))
        continue;
      lower_dirs_size += 1 + strlen(dir->source);
    }

    // lowerdir=<lower_dirs>
    // TODO: support upperdir
    const int LOWERDIR_PREFIX_SIZE = 9;
    size_t options_size = LOWERDIR_PREFIX_SIZE + root_length + lower_dirs_size;
    char *options = malloc(options_size + 1);
    if (!options) {
      errnof("malloc failed");
      return 1;
    }
    // TODO: do not add the rootfs as a lowerdir if there are no non-root mounts
    size_t offset = 0;
    memcpy(options + offset, "lowerdir=", LOWERDIR_PREFIX_SIZE);
    offset += LOWERDIR_PREFIX_SIZE;
    memcpy(options + offset, root, root_length);
    offset += root_length;

    for (int i = 0; i < dir_count; i++) {
      // TODO: handle upper dir properly
      struct dir *dir = &dirs[i];
      if (!is_root_mount(dir))
        continue;
      options[offset++] = ':';
      size_t len = strlen(dir->source);
      memcpy(options + offset, dir->source, len);
      offset += len;
    }
    if (offset != options_size) {
      errf("code bug: options_size %lu != offset %lu", options_size, offset);
      return 1;
    }
    options[offset] = '\0';
    logf("options = '%s'", options);

    if (-1 == loggy_mount("none", root, "overlay", options)) {
      // error already logged
      return 1;
    }
    free(options);
  }
  return 0; // success
}
#endif
