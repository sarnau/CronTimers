//
//  CronTimers.h
//
//  Created by Markus Fritze on 22/02/16.
//

#ifndef CronTimers_h
#define CronTimers_h

#include <time.h>

#define CRON_CHECKS          1 // during development: check if the cronStr is valid (saves about 300 bytes)
#define CRON_PRINT_SUPPORT   1 // allow printing the CronTimers structure - probably only useful during development. (saves about 1000 bytes)
#define CRON_YEAR_SUPPORT    0 // allow setting a timer based on a year – possible, but uncommon (saves about 100 bytes)
#define CRON_NAME_SUPPORT    1 // allow setting a time with english names for month/day of week - without this, they need to be provided as numbers, which saves about 400 bytes

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct {
    const char *cronStr;
    void (*triggerF)(int parameter);
    int parameter;
} CronTimer;

extern void cronTimersTriggerAtTime(time_t baseTime, const CronTimer *cronTimerEntry, int timerCount);

#if CRON_PRINT_SUPPORT
extern void cronTimersPrint(const CronTimer *cronTimerEntry, int timerCount);
#endif

#if defined(__cplusplus)
}
#endif

#endif
