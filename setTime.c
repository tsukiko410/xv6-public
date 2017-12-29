#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "date.h"

int main(int argc,char *argv[])
{
  int pid, priority, startHour, startMin, endHour, endMin, deadlineHour, deadlineMin;
 
  if(argc<9){
      printf(2,"Error: wrong input number.\n");
      exit();
  }
  pid = atoi(argv[1]);
  priority = atoi(argv[2]);
  if(priority < 0 || priority > 20){
      printf(2,"Error: wrong priority(0 < priority <= 20).\n");
      exit();
  }
  startHour = atoi(argv[3]);
  if(startHour < 0 || startHour > 24){
      printf(2,"Error: wrong time hour(0 < hour <= 24).\n");
      exit();
  }
  startMin = atoi(argv[4]);
  if(startHour < 0 || startHour > 60){
      printf(2,"Error: wrong time minute(0 < minute <= 60).\n");
      exit();
  }
  endHour = atoi(argv[5]);
  if(endHour < 0 || endHour > 24){
      printf(2,"Error: wrong time hour(0 < hour <= 24).\n");
      exit();
  }
  endMin = atoi(argv[6]);
  if(endHour < 0 || endHour > 60){
      printf(2,"Error: wrong time minute(0 < minute <= 60).\n");
      exit();
  }
  deadlineHour = atoi(argv[7]);
  if(deadlineHour < 0 || deadlineHour > 24){
      printf(2,"Error: wrong time hour(0 < hour <= 24).\n");
      exit();
  }
  deadlineMin = atoi(argv[8]);
  if(deadlineHour < 0 || deadlineHour > 60){
      printf(2,"Error: wrong time minute(0 < minute <= 60).\n");
      exit();
  }

  printf(1,"set: pid=%d, pr=%d, startTime=%d:%d, endTime=%d:%d, deadline=%d:%d\n", pid,priority, startHour, startMin, endHour, endMin, deadlineHour, deadlineMin);
  setTime(pid, priority, startHour, startMin, endHour, endMin, deadlineHour, deadlineMin);
  

  printf(1, "setTime end\n");
  exit();
}
