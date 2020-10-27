#include "kernel/types.h"
#include "user/user.h"

void cull(int p){
  int n;
  while (read(0, &n, sizeof(n)) == sizeof(n)) {
    if (n % p != 0) {
      write(1, &n, sizeof(n));
    }
  }
}

void redirect(int k, int pd[]){
  close(k);
  dup(pd[k]);
  close(pd[0]);
  close(pd[1]);
}

void sink(){
  int pd[2], p;
  if (read(0, &p, sizeof(p)) == sizeof(p)) {
    printf("prime %d\n", p);
    pipe(pd);
    if(fork() > 0){
      redirect(0, pd);
      sink();
    }
    else {
      redirect(1, pd);
      cull(p);
    }
  }
}

int main(int argc, char *argv[]){
  int pd[2];
  pipe(pd);
  if (fork() > 0) {
    redirect(0, pd);
    sink();
  }
  else {
    redirect(1, pd);
    for (int i = 2; i < 36; i++){
      write(1, &i, sizeof(i));
    }
  }
  exit();
}
