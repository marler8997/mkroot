# mkview

`mkview` is a small tool to create root filesystem "views". These "views" consist of directories and mount points into the underlying filesystem. They can be used to restrict access to the filesystem or overlay additional files on top of it.  A tool like `chroot` can be used to execute processes inside these views.

## Usage

```
mkview <view_dir> <dir>[:<target>]...
```

Once you've created a view, you can use `chroot` or the provided `inroot` command to execute a command inside it:

```
inroot <view_dir> <command>...
```

```
sudo chroot <view_dir> <command>...
```

You can use the `rmr` tool to cleanup a view directory. Unlike `rm`, `rmr` will unmount any directories and does not follow symbolic links, but instead removes the links.
```
rmr <view_dir>
```

## Examples

Make a view consisting only of the current directory:
```
mkview myview .
```

Make a view with the contents of 'myimage' as the rootfs and the current directory:
```
mkview myview myimage: .
```

The colon `:` character after `myimage` tells `mkview` to mount `myimage` at the root of the view.

Make a view with `/bin`, `/lib` and `/lib64` and chroot bash into it:
```
mkview myview /bin /lib /lib64
inroot myview /bin/bash
```

The same as the previous example, but mount `/bin` to `/hostbin`:
```
mkview myview /bin:hostbin /lib /lib64
inroot myview /hostbin/bash
```

This command will create a root directory that mirrors your current root directory but adds an additional overlay:
```
mkview myview myimage: /
inroot myview bash
# you can now access the files that are in the myimage directory as if myimage was installed to the root
```
