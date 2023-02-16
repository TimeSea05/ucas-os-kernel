#include <stdio.h>
#include <unistd.h>

static char buf[64];

int main() {
  int fd = sys_fopen("8MB.dat", O_RDWR);

  sys_lseek(fd, 0, SEEK_END);
  sys_fwrite(fd, "string at the end of the file.\n", 31);
  sys_lseek(fd, 0, SEEK_SET);
  sys_fwrite(fd, "string at the start of the file.\n", 33);

  sys_lseek(fd, -31, SEEK_END);
  sys_fread(fd, buf, 31);
  printf("%s", buf);

  sys_lseek(fd, 0, SEEK_SET);
  sys_fread(fd, buf, 33);
  printf("%s", buf);

  return 0;  
}