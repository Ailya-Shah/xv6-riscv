// fxtest: automated tests for find and xargs (CS-333 Assignment 2).
//
// Each test runs find or xargs as a child process, feeds it input through
// a pipe, captures everything it prints (stdout and stderr) through a
// second pipe, and compares that text and the exit status with what we
// expect. The program builds its own directory tree under /fxt and
// removes it at the end, so it can be run more than once.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"
#include "user/user.h"

#define MAXMADE 32

char made[MAXMADE][MAXPATH];    // everything we create, in creation order
int nmade = 0;
char out[1024];                 // captured output of the last run()
int passed = 0, total = 0;

// Create a file or directory and remember it for cleanup.
void
mk(char *path, int isdir)
{
  int fd;

  if (isdir) {
    if (mkdir(path) < 0) {
      fprintf(2, "fxtest: mkdir %s failed\n", path);
      exit(1);
    }
  } else {
    if ((fd = open(path, O_CREATE | O_RDWR)) < 0) {
      fprintf(2, "fxtest: create %s failed\n", path);
      exit(1);
    }
    close(fd);
  }
  strcpy(made[nmade++], path);
}

// Remove what we created, newest first (a directory must be empty
// before it can be unlinked).
void
cleanup(void)
{
  while (nmade > 0)
    unlink(made[--nmade]);
}

// Run argv in a child. 'in' is written to its stdin (pass "" for empty
// input). Its stdout and stderr both go into out[]. Returns its exit status.
int
run(char **argv, char *in)
{
  int op[2], ip[2], pid, n, len = 0, status;

  if (pipe(op) < 0 || pipe(ip) < 0) {
    fprintf(2, "fxtest: pipe failed\n");
    exit(1);
  }
  pid = fork();
  if (pid < 0) {
    fprintf(2, "fxtest: fork failed\n");
    exit(1);
  }
  if (pid == 0) {
    close(0); dup(ip[0]);       // stdin  <- input pipe
    close(1); dup(op[1]);       // stdout -> output pipe
    close(2); dup(op[1]);       // stderr -> output pipe
    close(ip[0]); close(ip[1]);
    close(op[0]); close(op[1]);
    exec(argv[0], argv);
    fprintf(2, "fxtest: exec %s failed\n", argv[0]);
    exit(127);
  }
  close(ip[0]);
  close(op[1]);
  write(ip[1], in, strlen(in));
  close(ip[1]);                 // child sees EOF after the input
  while ((n = read(op[0], out + len, sizeof(out) - 1 - len)) > 0)
    len += n;
  out[len] = 0;
  close(op[0]);
  wait(&status);
  return status;
}

void
check(char *name, char **argv, char *in, char *want, int wantstatus)
{
  int status = run(argv, in);

  total++;
  if (status == wantstatus && strcmp(out, want) == 0) {
    passed++;
    printf("PASS  %s\n", name);
  } else {
    printf("FAIL  %s\n", name);
    printf("  expected (status %d):\n%s", wantstatus, want);
    printf("  got      (status %d):\n%s", status, out);
  }
}

int
main(void)
{
  char deep[MAXPATH], want[MAXPATH + 2];
  static char longin[700];
  int i;

  chdir("/");                   // find and xargs are looked up in /
  if (mkdir("/fxt") < 0) {
    fprintf(2, "fxtest: /fxt already exists (an earlier run was "
               "interrupted?); run make clean && make qemu\n");
    exit(1);
  }
  strcpy(made[nmade++], "/fxt");

  // Build the test tree:
  //   /fxt/target                 file
  //   /fxt/a/target               file
  //   /fxt/a/b/target             directory (directories can match too)
  //   /fxt/abcdefghijklmn         file, name exactly DIRSIZ (14) chars
  //   /fxt/d/d/.../d/leaf         8 nested directories
  //   /fxtmark                    file directly under the root
  mk("/fxt/target", 0);
  mk("/fxt/a", 1);
  mk("/fxt/a/target", 0);
  mk("/fxt/a/b", 1);
  mk("/fxt/a/b/target", 1);
  mk("/fxt/abcdefghijklmn", 0);
  strcpy(deep, "/fxt");
  for (i = 0; i < 8; i++) {
    strcpy(deep + strlen(deep), "/d");
    mk(deep, 1);
  }
  strcpy(deep + strlen(deep), "/leaf");
  mk(deep, 0);
  mk("/fxtmark", 0);

  printf("--- find ---\n");
  {
    char *a1[] = { "find", "/fxt", "target", 0 };
    check("finds files and directories at every level", a1, "",
          "/fxt/target\n/fxt/a/target\n/fxt/a/b/target\n", 0);

    char *a2[] = { "find", "/fxt/target", "target", 0 };
    check("start path is itself a matching file", a2, "",
          "/fxt/target\n", 0);

    char *a3[] = { "find", "/fxt", "nosuchname", 0 };
    check("no match prints nothing", a3, "", "", 0);

    char *a4[] = { "find", "/fxt", "abcdefghijklmn", 0 };
    check("name of exactly 14 chars (DIRSIZ)", a4, "",
          "/fxt/abcdefghijklmn\n", 0);

    strcpy(want, deep);
    strcpy(want + strlen(want), "\n");
    char *a5[] = { "find", "/fxt", "leaf", 0 };
    check("10 nested calls deep (old version crashed at 7)", a5, "",
          want, 0);

    char *a6[] = { "find", "/", "fxtmark", 0 };
    check("search from / gives /fxtmark, not //fxtmark", a6, "",
          "/fxtmark\n", 0);

    char *a7[] = { "find", "/nope", "x", 0 };
    check("FAIL CASE: path does not exist", a7, "",
          "find: cannot open /nope\n", 1);

    char *a8[] = { "find", "/fxt", 0 };
    check("FAIL CASE: wrong number of arguments", a8, "",
          "usage: find <path> <name>\n", 1);
  }

  printf("--- xargs ---\n");
  {
    char *b1[] = { "xargs", "echo", "hi", 0 };
    check("runs the command once per input line", b1, "a\nb\n",
          "hi a\nhi b\n", 0);

    char *b2[] = { "xargs", "echo", 0 };
    check("splits a line into words on spaces/tabs", b2,
          "  one  two\tthree \n", "one two three\n", 0);

    char *b5[] = { "xargs", "wc", 0 };
    check("each word becomes a separate argument", b5,
          "/fxt/target /fxtmark\n",
          "0 0 0 /fxt/target\n0 0 0 /fxtmark\n", 0);

    check("skips blank and space-only lines", b2, "\n\nx\n   \n",
          "x\n", 0);

    check("last line without a newline still runs", b2, "last",
          "last\n", 0);

    check("EDGE CASE: empty input runs nothing", b2, "", "", 0);

    char *b3[] = { "xargs", 0 };
    check("FAIL CASE: no command given", b3, "",
          "usage: xargs cmd [args...]\n", 1);

    char *b4[] = { "xargs", "nosuchcmd", 0 };
    check("FAIL CASE: command does not exist", b4, "a\n",
          "xargs: exec nosuchcmd failed\n", 1);

    for (i = 0; i < 600; i++)
      longin[i] = 'a';
    strcpy(longin + 600, "\nok\n");
    check("FAIL CASE: over-long line is skipped, rest still runs", b2,
          longin, "xargs: input line too long, skipped\nok\n", 1);
  }

  cleanup();
  printf("--- %d/%d tests passed ---\n", passed, total);
  exit(passed == total ? 0 : 1);
}
