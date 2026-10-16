#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

// Read one line from stdin into buf (without the '\n').
// Returns the line length (0 for a blank line), or -1 at EOF with no data.
int
readline(char *buf, int max)
{
  int n = 0, r;
  char c;

  while ((r = read(0, &c, 1)) == 1) {
    if (c == '\n')
      break;
    if (n < max - 1)       // silently truncate over-long lines
      buf[n++] = c;
  }
  if (r <= 0 && n == 0)
    return -1;
  buf[n] = 0;
  return n;
}

int
main(int argc, char *argv[])
{
  char *args[MAXARG];
  char line[512];
  int i, n, pid;

  if (argc < 2) {
    fprintf(2, "usage: xargs cmd [args...]\n");
    exit(1);
  }
  if (argc - 1 + 1 >= MAXARG) {   // fixed args + one line + NULL terminator
    fprintf(2, "xargs: too many arguments\n");
    exit(1);
  }
  for (i = 1; i < argc; i++)
    args[i - 1] = argv[i];

  while ((n = readline(line, sizeof(line))) >= 0) {
    if (n == 0)
      continue;                    // skip blank lines
    args[argc - 1] = line;
    args[argc] = 0;                // exec needs a NULL-terminated array
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
    wait(0);
  }
  exit(0);
}
