//
//  CronTimers.c
//
//  Created by Markus Fritze on 22/02/16.
//

#include "CronTimers.h"
#include <ctype.h>
#if CRON_CHECKS || CRON_PRINT_SUPPORT
    #include <stdio.h>
#endif

#if CRON_NAME_SUPPORT
static const char *months[12] = {"jan","feb","mar","apr","may","jun","jul","aug","sep","oct","nov","dec"};
static const char *weekdays[7] = {"sun","mon","tue","wed","thu","fri","sat"};   // Sunday is the first element!
#endif

// Order of the fields in Crontab string
enum {
    Cron_Field_Second = 0,
    Cron_Field_Minute,
    Cron_Field_Hour,
    Cron_Field_Weekday,
    Cron_Field_DayWeek,
    Cron_Field_Month,
#if CRON_YEAR_SUPPORT
    Cron_Field_Year,
#endif
};


static void setbit(unsigned char *bitField, int bit)
{
    bitField[bit >> 3] |= 1 << (7 - (bit & 7));
}

static int checkbit(const unsigned char *bitField, int bit)
{
    return (bitField[bit >> 3] >> (7 - (bit & 7))) & 1;
}

static void clearallbits(unsigned char *bitField, int bitFieldSize)
{
    // clear all bits (quickly)
    for(int byte=0; byte<(bitFieldSize+7)>>3; ++byte)
        bitField[byte] = 0x00;
}

#if CRON_NAME_SUPPORT
static int isletter(char c)
{
    c = tolower(c);
    return c >= 'a' && c <= 'z';
}
#endif

static int iseos(char c)
{
    return c <= ' ';    // includes NUL, TAB, CR, LF, and SPC
}

static int getnum(const char **strP, int numOffset, int bitFieldSize
#if CRON_NAME_SUPPORT
    , const char **names
#endif
    )
{
    // is the string a decimal number?
    char c = **strP;
    if(isdigit(c)) {
        int num = 0;
        while(isdigit(c = **strP)) {
            ++*strP;
            num *= 10;
            num += c - '0';
        }
        return num + numOffset;     // the offset makes the input zero-based

    // or do we have a list of strings?
#if CRON_NAME_SUPPORT
    } else if(names && isletter(c)) {
        // iterate over all strings to compare
        for(int strIndex=0; strIndex<bitFieldSize; ++strIndex) {
            const char *str = names[strIndex];
            int found = 1;
            int len;
            // iterate over a single string to compare
            for(len=0; str[len] != 0; ++len) {
                if(str[len] != tolower((*strP)[len])) { // the input is case-insentive, the string list has to be lower case
                    found = 0;
                    break;
                }
            }
            if(found) {
                *strP += len;   // length of the found string
                return strIndex;    // no offset, the list is always zero-based
            }
        }
#endif
    }

    // unknown
    return -1;
}

// skip fields to find a specific field
static const char *findField(const char *cronString, int fieldIndex)
{
    while(fieldIndex-- > 0) {
        while(!iseos(cronString[0])) {
            ++cronString;
        }
        while(iseos(cronString[0])) {
            if(!cronString[0])
                break;
            ++cronString;
        }
    }
    return cronString;
}

static void parseCronString(const char *cronString, int fieldIndex, unsigned char *bitField, int bitFieldSize, int numOffset
#if CRON_NAME_SUPPORT
    , const char **names
#endif
    )
{
    // clear all bits (quickly)
    clearallbits(bitField, bitFieldSize);

    if(!cronString) return;
    cronString = findField(cronString, fieldIndex);

    while(!iseos(cronString[0])) {
        int firstValue = -1;  // not set
        int lastValue = -1;

        // '?' = nothing (used to filter day/weekday)
        if(cronString[0] == '?') {
            ++cronString;
            break;

        // '*' = every possible value
        } else if(cronString[0] == '*') {
            ++cronString;
            firstValue = 0;
            lastValue = bitFieldSize - 1;

        } else if(isdigit(cronString[0])
#if CRON_NAME_SUPPORT
            || (names && isletter(cronString[0]))
#endif
            ) {
            firstValue = getnum(&cronString, numOffset, bitFieldSize
#if CRON_NAME_SUPPORT
                , names
#endif
                );
            if(firstValue >= bitFieldSize) {
#if CRON_CHECKS
                printf("### size: %d > %d\n", firstValue, bitFieldSize);
#endif
                return;
            }
            // a range?
            if(cronString[0] == '-') {
                ++cronString;
                lastValue = getnum(&cronString, numOffset, bitFieldSize
#if CRON_NAME_SUPPORT
                    , names
#endif
                    );
                if(lastValue < 0)
                    lastValue = bitFieldSize - 1;
                if(firstValue > lastValue) {
#if CRON_CHECKS
                    printf("### order: %d > %d\n", firstValue, lastValue);
#endif
                    return;
                }
                if(lastValue >= bitFieldSize) {
#if CRON_CHECKS
                    printf("### size: %d > %d\n", lastValue, bitFieldSize);
#endif
                    return;
                }
            }
        }
        if(firstValue < 0) {
#if CRON_CHECKS
            printf("### first not set\n");
#endif
            return;
        }

        // skip offset
        int skipOffset = 1;
        if(cronString[0] == '/') {
            ++cronString;
            skipOffset = getnum(&cronString, 0, 0
#if CRON_NAME_SUPPORT
                , NULL
#endif
                );
            if(skipOffset == 0) {
#if CRON_CHECKS
                printf("### skip == 0\n");
#endif
                return;
            }
            // a skip without a last value uses the full size as the upper range
            if(lastValue < 0)
                lastValue = bitFieldSize - 1;
        }
        // if the last was not defined, we default to a single value
        if(lastValue < 0)
            lastValue = firstValue;

        // set the matching bits
        do {
            setbit(bitField, firstValue);
            firstValue += skipOffset;
        } while(firstValue <= lastValue);

        // a list?
        if(cronString[0] == ',') {
            ++cronString;
        } else if(!iseos(cronString[0])) {
#if CRON_CHECKS
            printf("### unknown character: [%c]", cronString[0]);
#endif
            return;
        }
    }
}

static void     weekNumberToDays(const unsigned char *weeks, unsigned char *days, int bitFieldSize)
{
    // clear all bits (quickly)
    clearallbits(days, bitFieldSize);

    // check the week bitmask and set all days within the days bitmask for any given week
    for(int weekIndex=0; weekIndex<5; ++weekIndex) {
        if(checkbit(weeks, weekIndex)) {
            for(int day=0; day<7; ++day) {
                int bit = weekIndex * 7 + day;
                if(bit >= bitFieldSize)
                    break;
                setbit(days, bit);
            }
        }
    }
}

void cronTimersTriggerAtTime(time_t baseTime, const CronTimer *cronTimerEntry, int timerCount)
{
    struct tm tm;
    localtime_r(&baseTime, &tm);

    for(int timerIndex=0; timerIndex<timerCount; ++timerIndex, ++cronTimerEntry) {
        unsigned char   bits[64/8];    // 64 bits are enough for any type below (minute/seconds are 60)
        const char      *cronStr = cronTimerEntry->cronStr;
        int alarmActive = 1;
#if CRON_YEAR_SUPPORT
        const int       yearBase = 2015;
        parseCronString(cronStr, Cron_Field_Year, bits, 30, -yearBase
#if CRON_NAME_SUPPORT
            , NULL
#endif
            );
        if(!checkbit(bits, tm.tm_year + 1900 - yearBase))   // tm_year starts at 1900
            alarmActive = 0;
        if(alarmActive)
#endif
        {
            parseCronString(cronStr, Cron_Field_Month, bits, 12, -1
#if CRON_NAME_SUPPORT
                , months
#endif
                );
            if(!checkbit(bits, tm.tm_mon))  // tm_mon starts at 0
                alarmActive = 0;
            if(alarmActive) {
                int isWeek;
                const char *fieldHeader = findField(cronStr, Cron_Field_DayWeek);
                if(fieldHeader && fieldHeader[0] == '#') {     // week number, instead of specific days of the month
                    unsigned char   weeks[1];
                    parseCronString(++fieldHeader, 0, weeks, 5, -1
#if CRON_NAME_SUPPORT
                        , NULL
#endif
                        );
                    weekNumberToDays(weeks, bits, 31);
                    isWeek = 1;
                } else {
                    parseCronString(cronStr, Cron_Field_DayWeek, bits, 31, -1
#if CRON_NAME_SUPPORT
                        , NULL
#endif
                        );
                    isWeek = 0;
                }
                unsigned char   weekday[1]; // 7 bits needed
                parseCronString(cronStr, Cron_Field_Weekday, weekday, 7, 0
#if CRON_NAME_SUPPORT
                    , weekdays
#endif
                    );
                if(isWeek) {    // If we have a weekday within week n of the month, then _both_ conditions have to be true
                    if(!(checkbit(bits, tm.tm_mday - 1) && checkbit(weekday, tm.tm_wday)))    // only if day of month _and_ weekday are invalid, we do fail (tm_mday starts at 1, tm_wday at 0)
                        alarmActive = 0;
                } else {        // for fixed days and weekdays, _either_ one of the conditions have to be true
                    if(!(checkbit(bits, tm.tm_mday - 1) || checkbit(weekday, tm.tm_wday)))    // only if day of month _and_ weekday are invalid, we do fail (tm_mday starts at 1, tm_wday at 0)
                        alarmActive = 0;
                }
                if(alarmActive) {
                    parseCronString(cronStr, Cron_Field_Hour, bits, 24, 0
#if CRON_NAME_SUPPORT
                        , NULL
#endif
                        );
                    if(!checkbit(bits, tm.tm_hour))
                        alarmActive = 0;
                    if(alarmActive) {
                        parseCronString(cronStr, Cron_Field_Minute, bits, 60, 0
#if CRON_NAME_SUPPORT
                            , NULL
#endif
                            );
                        if(!checkbit(bits, tm.tm_min))
                            alarmActive = 0;
                        if(alarmActive) {
                            parseCronString(cronStr, Cron_Field_Second, bits, 60, 0
#if CRON_NAME_SUPPORT
                                , NULL
#endif
                                );
                            if(!checkbit(bits, tm.tm_sec))
                                alarmActive = 0;
                        }
                    }
                }
            }
        }
        if(alarmActive)
          cronTimerEntry->triggerF(cronTimerEntry->parameter);
    }
}

#if CRON_PRINT_SUPPORT
static void printCronEntryLabel(int bit, int offset
#if CRON_NAME_SUPPORT
    , const char **bitNames
#endif
    )
{
#if CRON_NAME_SUPPORT
    if(bitNames) {
        printf("%s",bitNames[bit]);
    } else
#endif
    {
        printf("%d",bit+offset);
    }
}

static void printCronEntry(const unsigned char *bits, int bitFieldSize, const char *header, int offset
#if CRON_NAME_SUPPORT
    , const char **bitNames
#endif
    , const char *footer)
{
    printf("%s = ", header);
    int allSet = 1;
    int noneSet = 1;
    for(int bit=0; bit<bitFieldSize; ++bit) {
        if(!checkbit(bits, bit))
            allSet = 0;
        else
            noneSet = 0;
    }
    if(allSet) {
        printf("any");
    } else if(noneSet) {
        printf("never");
    } else {
        int first = 1;
        for(int bit=0; bit<bitFieldSize; ++bit) {
            if(checkbit(bits, bit)) {
                if(!first)
                    putchar(',');
                int bitCount = 0;
                for(; bit+bitCount<bitFieldSize && checkbit(bits, bit+bitCount); ++bitCount) {
                }
                printCronEntryLabel(bit, offset
#if CRON_NAME_SUPPORT
                    , bitNames
#endif
                    );
                if(bitCount > 1) {
                    putchar(bitCount > 2 ? '-' : ',');
                    printCronEntryLabel(bit+bitCount-1, offset
#if CRON_NAME_SUPPORT
                        , bitNames
#endif
                        );
                    bit += bitCount;
                }
                first = 0;
            }
        }
    }
    if(footer)
        printf("%s", footer);
    else
        printf("\n");
}

void cronTimersPrint(const CronTimer *cronTimerEntry, int timerCount)
{
    for(int timerIndex=0; timerIndex<timerCount; ++timerIndex, ++cronTimerEntry) {
        printf("CronTimer #%d\n", timerIndex);
        unsigned char   bits[8];    // 64 bits are enough for any type below (minute/seconds are 60)
        const char      *cronStr = cronTimerEntry->cronStr;
#if CRON_YEAR_SUPPORT
        const int       yearBase = 2015;
        parseCronString(cronStr, Cron_Field_Year, bits, 30, -yearBase
#if CRON_NAME_SUPPORT
            , NULL
#endif
            );
        printCronEntry(bits, 30, "years", yearBase
#if CRON_NAME_SUPPORT
            , NULL
#endif
            , NULL);
#endif
        parseCronString(cronStr, Cron_Field_Month, bits, 12, -1
#if CRON_NAME_SUPPORT
            , months
#endif
            );
        printCronEntry(bits, 12, "mon", 1
#if CRON_NAME_SUPPORT
            , months
#endif
            , NULL);
        const char *fieldHeader = findField(cronStr, Cron_Field_DayWeek);
        if(fieldHeader && fieldHeader[0] == '#') {     // week number, instead of specific days of the month
            unsigned char   weeks[1];
            parseCronString(++fieldHeader, 0, weeks, 5, -1
#if CRON_NAME_SUPPORT
                , NULL
#endif
                );
            printCronEntry(weeks, 5, "week", 1
#if CRON_NAME_SUPPORT
                , NULL
#endif
                , " and ");
        } else {
            parseCronString(cronStr, Cron_Field_DayWeek, bits, 31, -1
#if CRON_NAME_SUPPORT
                , NULL
#endif
                );
            printCronEntry(bits, 31, "day", 1
#if CRON_NAME_SUPPORT
                , NULL
#endif
                , " or ");
        }
        parseCronString(cronStr, Cron_Field_Weekday, bits, 7, 0
#if CRON_NAME_SUPPORT
            , weekdays
#endif
            );
        printCronEntry(bits, 7, "wday", 0
#if CRON_NAME_SUPPORT
            , weekdays
#endif
            , NULL);
        parseCronString(cronStr, Cron_Field_Hour, bits, 24, 0
#if CRON_NAME_SUPPORT
            , NULL
#endif
            );
        printCronEntry(bits, 24, "hour", 0
#if CRON_NAME_SUPPORT
            , NULL
#endif
            , NULL);
        parseCronString(cronStr, Cron_Field_Minute, bits, 60, 0
#if CRON_NAME_SUPPORT
            , NULL
#endif
            );
        printCronEntry(bits, 60, "min", 0
#if CRON_NAME_SUPPORT
            , NULL
#endif
            , NULL);
        parseCronString(cronStr, Cron_Field_Second, bits, 60, 0
#if CRON_NAME_SUPPORT
            , NULL
#endif
            );
        printCronEntry(bits, 60, "sec", 0
#if CRON_NAME_SUPPORT
            , NULL
#endif
            , NULL);
        printf("\n");
    }
    printf("\n");
}
#endif
