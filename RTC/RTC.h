#ifndef RTC_H 
#define RTC_H
#ifdef __cplusplus
#define restrict __restrict__
extern "C" {
#endif
/*
This library provides two options for the source of the RTC. One is the DS3231 module
and the other is the internal Timer 2 peripheral driven by a 32kHz external crystal.
This option is chosen by commenting out the undesired macro below.
*/

#define MAX_TIMERS 30

#define RTC_FROM_TOSC
//#define RTC_FROM_DS3231

// Choose to disable the Unix time variable to increase performance
//#define USE_UNIX_TIME

/* The time and date fields will be according to the time zone set by the setTimeZone() function.
However the unix field will always have unix time which is based on UTC. */
struct Time
{
	uint8_t sec;
	uint8_t min;
	uint8_t hour;
	uint8_t day;
	uint8_t weekday;
	uint8_t mon;
	uint16_t year;
	#ifdef USE_UNIX_TIME
	uint64_t unix;
	#endif
};

/*
Usage of the functions:
RTC.init();
RTC.setTimeZone(2); // UTC+2
RTC.setTime(34, 23, 5, 23, 6, 2020);
RTC.read(&time);
*/

struct RealTimeClock
{
	void (*const init)(void);
	void (*const read)(struct Time *const restrict time);
	void (*const setTimeZone)(const int8_t UTCoffset);
	void (*const setTime)(const uint8_t s, const uint8_t min, const uint8_t h, const uint8_t d, const uint8_t mon, const uint16_t y);
	uint8_t (*const dayOfWeek)(uint16_t d, const uint8_t mon, uint16_t y);
	int8_t (*const setTimer)(const uint32_t seconds);
	int8_t (*const resetTimer)(const int8_t timer, const uint32_t seconds);
	int8_t (*const timerDone)(const int8_t timer);
};

extern const struct RealTimeClock RTC;

	
#ifdef __cplusplus
}
#endif
#endif // RTC_H
