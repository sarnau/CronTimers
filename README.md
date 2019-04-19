# CronTimers

CronTimers is a C library for Arduino

## Features

* Parser for Cron-style strings to execute a list timers
* Support for seconds, minutes, hours, day, weekday, week number within a month, month and year
* Support for lists or ranges of values or wildcards and "every n"
* Function to display the Cron-style string in clear text to allow debugging the input
* Very small codebase, which can be further minimized by disabling debug code and/or strings for weekdays and months.
* The number of active timers is only limited by the memory in the device
* [MIT License](https://en.wikipedia.org/wiki/MIT_License)

## Examples

    "5-7,15-45 * */6 1-3 #2,4 2/3 *"

Execute the trigger when the following conditions are met:

* "5-7,15-45" Seconds is between 5-7 or 15-45 (typically called every second)
* "*": Minutes are ignored
* "*/6": Every 6 hours (0, 6, 12, 18, which matches 12am, 6am, 12pm, 6pm)
* "1-3": Between Monday-Wednesday
* "#2,4": In week number 2 and 4 within a month
* "2/3": Only in February and March 
* "*": The Year is ignored = every year

Another example:

    "50-58/2 * * ? * * *"

Execute the trigger at 50s, 52s, 54s, 56s and 58s every minute, every hour, etc. - "?" = week day is ignored.

Another example:

    "* * 4 * #2,4 * *"

At 4am in week 2 and 4 in every months. Warning: some months have 5 weeks!

Another warning: if you pick a time between 2-3am in the morning: during the switch between summer/wintertime this hour might be missing _or_ happen twice. You can avoid that by always using UTC as the time base for all timers and only convert to local time for display purposes.