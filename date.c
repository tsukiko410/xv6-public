/* add for date function*/

#include "types.h"
#include "user.h"
#include "date.h"

int main(int argc, char *argv[])
{
    struct rtcdate r;

    if(date(&r))
    {
	printf(2, "date error.\n");
	exit();
    }
    
    int timeZoneHour = r.hour + 8;
    if(timeZoneHour >= 24)
	timeZoneHour -= 24;

    printf(1, "%d-%d-%d %d:%d:%d", r.year, r.month, r.day, timeZoneHour, r.minute, r.second);
    
    exit();
}
