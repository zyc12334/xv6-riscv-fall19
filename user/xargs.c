#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(2, "Usage: xargs command\n");
    exit();
  }
  char *_argv[MAXARG];
  for (int i = 0; i < argc; i++)
    _argv[i] = argv[i+1];
  char buf[1024];
  int stat = 1;
  while (stat) {
    int lst_arg = 0;
    int buf_cnt = 0;
    int argv_cnt = argc - 1;
    char ch;
    while (1) {
      stat = read(0, &ch, 1);
      if (stat == 0)
        exit();
      if (ch == ' ' || ch == '\n') {
        buf[buf_cnt++] = 0;
        _argv[argv_cnt++] = &buf[lst_arg];
        lst_arg = buf_cnt;
        if (ch == '\n') break;
      }
      else buf[buf_cnt++] = ch;
    }
    if (fork() == 0) {
      exec(_argv[0], _argv);
    }
    else {
      wait();
    }
  }
  exit();
}
