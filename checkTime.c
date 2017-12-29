#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "date.h"

int main(int argc,char *argv[])
{
  struct rtcdate r;
    if(date(&r))
    {
	printf(2, "date error.\n");
	exit();
    }
  int hour = r.hour + 8, min = r.minute;
  printf(1, "checking time\n");
  printf(1, "%d:%d\n", hour, min);
  checkTime(hour, min);
  exit();
}
