#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]){
  int parent_fd[2], child_fd[2];
  pipe(parent_fd);
  pipe(child_fd);
  char c;
  int n;
  if(fork() == 0){ //child
    close(parent_fd[1]);
    close(child_fd[0]);
    n = read(parent_fd[0], &c, 1);
    if(n != 1){
      fprintf(2, "Child read error! \n");
      exit();
    }
    printf("%d: received ping\n", getpid());
    write(child_fd[1], &c, 1);
    close(parent_fd[0]);
    close(child_fd[1]);
  }
  else{
    close(parent_fd[0]);
    close(child_fd[1]);
    write(parent_fd[1], &c, 1);
    n = read(child_fd[0], &c, 1);
    if(n != 1){
      fprintf(2,"Parent read error! \n");
      exit();
    }
    printf("%d: received pong\n", getpid());
    close(child_fd[0]);
    close(parent_fd[1]);
    wait();
  }
  exit();
}
