#include <stdio.h>
#include <unistd.h>
#define ARR_SIZE 2000

static char blank[] = {"                                                                                "};

int sum;
int arr[ARR_SIZE];

int print_location_main = 6;
int print_location_thread1 = 7;
int print_location_thread2 = 8;

int is_thread1_finished;
int is_thread2_finished;

void thread1() {
  int sum1 = 0;

  for (int i = 0; i < 1000; i++) {
    sum1 += arr[i];

    sys_move_cursor(0, print_location_thread1);
    printf("> [TASK] Thread 1 is running, sum1: %d\n", sum1);
  }

  sys_move_cursor(0, print_location_thread1);
  printf("%s", blank);
  sys_move_cursor(0, print_location_thread1);
  printf("> [TASK] Thread 1 finished, sum1: %d\n", sum1);

  is_thread1_finished = 1;
  sum += sum1;
  
  while (1) {
    asm volatile("wfi");
  }
}

void thread2() {
  int sum2 = 0;
  
  for (int i = 1000; i < 2000; i++) {
    sum2 += arr[i];

    sys_move_cursor(0, print_location_thread2);
    printf("> [TASK] Thread 2 is running, sum2: %d\n", sum2);
  }

  sys_move_cursor(0, print_location_thread2);
  printf("%s", blank);
  sys_move_cursor(0, print_location_thread2);
  printf("> [TASK] Thread 2 finished, sum2: %d\n", sum2);

  sum += sum2;
  is_thread2_finished = 1;
  
  while (1) {
    asm volatile("wfi");
  }
}

int main() {
  sys_move_cursor(0, print_location_main);
  printf("> [TASK] Entering main thread.\n");

  for (int i = 0; i < ARR_SIZE; i++) {
    arr[i] = i + 1;
  }

  sys_thread_create(thread1);
  sys_thread_create(thread2);
  sys_move_cursor(0, print_location_main);
  printf("> [TASK] Main thread: finish creating 2 sub threads.\n");

  while (!(is_thread1_finished && is_thread2_finished)) {
    sys_move_cursor(0, print_location_main);
    printf("> [TASK] Main thread: waiting for the 2 sub threads to finish.\n");
  }

  sys_move_cursor(0, print_location_main);
  printf("%s", blank);
  sys_move_cursor(0, print_location_main);
  printf("> [TASK] Main thread finished. sum: %d\n", sum);

  while (1) {
     asm volatile("wfi");
  }
}