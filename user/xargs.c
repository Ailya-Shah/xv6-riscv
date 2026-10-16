#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

// Read one line from stdin into buf (without the '\n').
// Returns the line length (0 for a blank line), or -1 at EOF with no data.
// Sets *toolong to 1 if the line did not fit in buf; the rest of that
// line is read and thrown away so the next call starts on a new line.
int
readline(char *buf, int max, int *toolong)
{
  int n = 0, r;
  char c;

  *toolong = 0;
  while ((r = read(0, &c, 1)) == 1) {
    if (c == '\n')
      break;
    if (n < max - 1)
      buf[n++] = c;
    else
      *toolong = 1;
  }
  if (r <= 0 && n == 0 && !*toolong)
    return -1;
  buf[n] = 0;
  return n;
}

// Split line in place on spaces/tabs, storing word pointers in args
// starting at index 'start'. Returns the new argument count, or -1 if
// the words would not fit in args[] (one slot is kept for the NULL).
int
split(char *line, char *args[], int start)
{
  int n = start;
  char *p = line;

  for (;;) {
    while (*p == ' ' || *p == '\t')
      p++;
    if (*p == 0)
      break;
    if (n >= MAXARG - 1)
      return -1;
    args[n++] = p;
    while (*p && *p != ' ' && *p != '\t')
      p++;
    if (*p)
      *p++ = 0;                     // terminate this word
  }
  return n;
}

int
main(int argc, char *argv[])
{
  char *args[MAXARG];
  char line[512];
  int i, n, nargs, pid, status, toolong, failed = 0;

  if (argc < 2) {
    fprintf(2, "usage: xargs cmd [args...]\n");
    exit(1);
  }
  if (argc - 1 >= MAXARG - 1) {     // need room for at least one word + NULL
    fprintf(2, "xargs: too many arguments\n");
    exit(1);
  }
  for (i = 1; i < argc; i++)
    args[i - 1] = argv[i];

  while ((n = readline(line, sizeof(line), &toolong)) >= 0) {
    if (toolong) {
      fprintf(2, "xargs: input line too long, skipped\n");
      failed = 1;
      continue;
    }
    nargs = split(line, args, argc - 1);
    if (nargs < 0) {
      fprintf(2, "xargs: too many arguments on one line, skipped\n");
      failed = 1;
      continue;
    }
    if (nargs == argc - 1)
      continue;                     // blank line: no new words
    args[nargs] = 0;                // exec needs a NULL-terminated array

    pid = fork();
    if (pid < 0) {
      fprintf(2, "xargs: fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      exec(args[0], args);
      fprintf(2, "xargs: exec %s failed\n", args[0]);
      exit(1);
    }
    wait(&status);
    if (status != 0)
      failed = 1;
  }
  exit(failed);
}
