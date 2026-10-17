#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"
#include "user/user.h"

// The path being searched. It is a single global buffer that grows as
// find() descends into a directory and is cut back when it returns, so
// each recursion level does not need its own 512-byte copy on the stack.
// (xv6 gives a user program only one 4096-byte stack page; with a
// 512-byte buffer per call, the sixth nested call overflowed it.)
// MAXPATH is the longest path the kernel's open() accepts.
char path[MAXPATH];
int errors = 0;

// Return pointer to the last component of p ("a/b/c" -> "c").
char *
basename(char *p)
{
  char *last = p;

  for (; *p; p++)
    if (*p == '/')
      last = p + 1;
  return last;
}

// Print every path under 'path' (including 'path' itself) whose last
// component equals 'name'.
void
find(char *name)
{
  int fd, len, base;
  struct dirent de;
  struct stat st;

  if ((fd = open(path, O_RDONLY)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    errors = 1;
    return;
  }
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    errors = 1;
    close(fd);
    return;
  }

  if (strcmp(basename(path), name) == 0)
    printf("%s\n", path);

  if (st.type == T_DIR) {
    len = strlen(path);
    if (len + 1 + DIRSIZ + 1 > sizeof(path)) {
      fprintf(2, "find: path too long: %s\n", path);
      errors = 1;
      close(fd);
      return;
    }
    base = len;
    if (len == 0 || path[len - 1] != '/')   // avoid "//x" when path is "/"
      path[len++] = '/';
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0)
        continue;
      memmove(path + len, de.name, DIRSIZ);
      path[len + DIRSIZ] = 0;   // name[] is not NUL-terminated if it is 14 chars long
      if (strcmp(path + len, ".") == 0 || strcmp(path + len, "..") == 0)
        continue;
      find(name);
    }
    path[base] = 0;             // restore path for the caller
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  if (argc != 3) {
    fprintf(2, "usage: find <path> <name>\n");
    exit(1);
  }
  if (strlen(argv[1]) >= sizeof(path)) {
    fprintf(2, "find: path too long\n");
    exit(1);
  }
  strcpy(path, argv[1]);
  find(argv[2]);
  exit(errors);
}
