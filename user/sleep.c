#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]){
  if(argc < 2){
    fprintf(2, "usage: sleep seconds...\n");
    exit();
  }
  int sleep_time = atoi(argv[1]);
  if(sleep_time <= 0){
    fprintf(2, "Invalid input %s\n", argv[1]);
  }
  else{
    sleep(sleep_time);
  }
  exit();
}
