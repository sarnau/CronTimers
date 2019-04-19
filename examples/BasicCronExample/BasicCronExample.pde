#include <CronTimers.h>
#include <stdio.h>

static void trigger(int parameter)
{
    printf("%d trigger #%d\n", parameter);
}

static const CronTimer   cronTimers[] = {
    // second,minute,hour,weekday,day/week#,month,year
    { "5-7,15-45 * */6 1-3 #2,4 2/3 *", trigger, 1 },
    { "50-58/2 * * ? * * *", trigger, 2 },
};


void setup()
{
#if CRON_PRINT_SUPPORT
    cronTimersPrint(cronTimers, sizeof(cronTimers)/sizeof(cronTimers[0]));
#endif
}

void loop()
{
    static time_t   lastTime;
    time_t currentTime = time(NULL);
    if(currentTime != lastTime) {
        lastTime = currentTime;

#if 1
        struct tm tm;
        localtime_r(&currentTime, &tm);
        printf("local: %s", asctime(&tm));
#endif
        cronTimersTriggerAtTime(currentTime, cronTimers, sizeof(cronTimers)/sizeof(cronTimers[0]));
    }
}
