/* add for check CPU alarms*/

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "date.h"


void cycle()
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
/*   
 struct rtcdate r;

    if(date(&r))
    {
	printf(2, "date error.\n");
	exit();
    }
    
    int timeZoneHour = r.hour + 8;
    if(timeZoneHour >= 24)
	timeZoneHour -= 24;
    int min = r.minute;
    checkTime(timeZoneHour, min);
    printf(1, "%d-%d-%d %d:%d:%d\n", r.year, r.month, r.day, timeZoneHour, r.minute, r.second);
*/
}

int main(int argc, char *argv[])
{
    printf(1, "Start checking alarm.\n");
    alarm(1200, cycle);	//1sec
   
 for(int i = 0; i < 2147483647; i++)
    {
	if((i % 4194304) == 0)
	    write(2, ".", 1);
    }

    printf(1, "time check open.\n");
    exit();
}
