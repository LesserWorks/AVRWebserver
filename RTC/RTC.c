#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "RTC.h"
#ifdef RTC_FROM_DS3231
#include "rtc3231.h"
#endif

#if MAX_TIMERS > 127 || MAX_TIMERS < 1
#error "Invalid number of timers"
#endif

#define disableRTCint() (TIMSK2 = 0) // disable RTC interrupt
#define enableRTCint() (TIMSK2 = 1 << TOIE2) // disable RTC interrupt

struct Timer 
{
	uint8_t inUse;
	uint32_t seconds;
};

static struct Timer timers[MAX_TIMERS] = {0};
static int8_t timeZone = 0; // Start at UTC+0

static void RTCinit(void);
static void RTCread(struct Time *const restrict time);
static void RTCsetTimeZone(const int8_t UTCoffset);
static void RTCsetTime(const uint8_t s, const uint8_t min, const uint8_t h, const uint8_t d, const uint8_t mon, const uint16_t y);
static uint8_t dayOfWeek(uint16_t d, const uint8_t m, uint16_t y);
static uint8_t notLeap(uint16_t year);
static void addTimeZoneOffset(struct Time *const dest, const struct Time *const src, const int8_t timeZone);
static int8_t RTCsetTimer(const uint32_t seconds);
static int8_t RTCtimerDone(const int8_t timer);

#ifdef USE_UNIX_TIME
static uint64_t calcUnix(const struct Time *const restrict time);
#endif

const struct RealTimeClock RTC = {&RTCinit, &RTCread, &RTCsetTimeZone, &RTCsetTime, &dayOfWeek, &RTCsetTimer, &RTCtimerDone};
#ifdef RTC_FROM_TOSC
static volatile struct Time rtc;
#endif

static void RTCinit(void)
{
	#ifdef RTC_FROM_TOSC

	_delay_ms(1000);
	disableRTCint();
	ASSR = 1 << AS2; // Clock from TOSC pins
	TCNT2 = 0;
	TCCR2A = 0; // Normal counting up mode
	TCCR2B = (1 << CS22) | (1 << CS20); // Prescaler at 128
	while (ASSR & ((1<<TCN2UB)|(1<<OCR2AUB)|(1<<OCR2BUB)|(1<<TCR2AUB)|(1<<TCR2BUB)));
	// With 32.768 kHz crystal, 128 prescaler, and 8-bit counter, it will
	// interrupt once every second.
	sei();

	#elif defined(RTC_FROM_DS3231)

	rtc3231_init();

	#endif
}

static void RTCsetTimeZone(const int8_t UTCoffset)
{
	timeZone = UTCoffset;
}

static void RTCsetTime(const uint8_t s, const uint8_t min, const uint8_t h, const uint8_t d, const uint8_t mon, const uint16_t y)
{
	const uint8_t weekday = dayOfWeek(d, mon, y);
	// 0 = Sunday, 6 = Shabbat
	#ifdef USE_UNIX_TIME
	struct Time temp = {s, min, h, d, weekday, mon, y, 0};
	#else
	struct Time temp = {s, min, h, d, weekday, mon, y};
	#endif
	addTimeZoneOffset(&temp, &temp, -timeZone); // Invert the timezone offset to get us to UTC from the given local time

	#ifdef RTC_FROM_TOSC
	#ifdef USE_UNIX_TIME
	temp.unix = calcUnix(temp);
	#endif
	disableRTCint();
	rtc = temp;
	enableRTCint();

	#elif defined(RTC_FROM_DS3231)

	struct rtc_time t = {temp.sec, temp.min, temp.hour};
	struct rtc_date date = {temp.weekday, temp.day, temp.mon, temp.year - 2000U};
	rtc3231_write_time(&t);
	rtc3231_write_date(&date);

	#endif
}

static void RTCread(struct Time *const restrict time)
{
	#ifdef RTC_FROM_TOSC

	disableRTCint();
	// Cast rtc to non-volatile since we disabled interrupts
	addTimeZoneOffset(time, (struct Time *)&rtc, timeZone);
	enableRTCint();

	#elif defined(RTC_FROM_DS3231)

	struct rtc_time t;
	struct rtc_date d;
	rtc3231_read_datetime(&t, &d);
	
	#ifdef USE_UNIX_TIME
	const struct Time temp = {t.sec, t.min, t.hour, d.day, d.wday, d.month, d.year + 2000U, 0};
	temp->unix = calcUnix(temp);
	#else
	const struct Time temp = {t.sec, t.min, t.hour, d.day, d.wday, d.month, d.year + 2000U};
	#endif
	addTimeZoneOffset(time, &temp, timeZone);
	#endif
}

static int8_t RTCsetTimer(const uint32_t seconds)
{
	disableRTCint();
	for(int8_t i = 0; i < MAX_TIMERS; i++) 
	{
		if(!timers[i].inUse)
		{
			timers[i].seconds = seconds;
			timers[i].inUse = 1;
			enableRTCint();
			return i;
		}
	}
	enableRTCint();
	return -1;
}

static int8_t RTCtimerDone(const int8_t timer)
{
	disableRTCint();
	if(timer < MAX_TIMERS && timer >= 0 && timers[timer].inUse) {
		if(timers[timer].seconds == 0)
		{
			timers[timer].inUse = 0; // When we read a zero timer, it releases the timer
			enableRTCint();
			return 1; // true that timer finished
		}
		else
		{
			enableRTCint();
			return 0; // timer did not finish
		}
	}
	enableRTCint();
	return -1;
}

#ifdef RTC_FROM_TOSC
ISR(TIMER2_OVF_vect)
{
	for(uint8_t i = 0; i < MAX_TIMERS; i++)
	{
		if(timers[i].inUse)
			timers[i].seconds -= 1; // Decrement all active timers
	}
	#ifdef USE_UNIX_TIME
	rtc.unix++;
	#endif
	if (++rtc.sec == 60)
	{
		rtc.sec = 0;
		if (++rtc.min == 60)
		{
			rtc.min = 0;
			if (++rtc.hour == 24)
			{
				rtc.weekday = (rtc.weekday + 1) % 7;
				rtc.hour = 0;
				if (++rtc.day == 32)
				{
					rtc.mon++;
					rtc.day = 1;
				}
				else if (rtc.day == 31)
				{
					if ((rtc.mon == 4) || (rtc.mon == 6) || (rtc.mon == 9) || (rtc.mon == 11))
					{
						rtc.mon++;
						rtc.day = 1;
					}
				}
				else if (rtc.day == 30)
				{
					if(rtc.mon == 2)
					{
						rtc.mon++;
						rtc.day = 1;
					}
				}
				else if (rtc.day == 29)
				{
					if((rtc.mon == 2) && notLeap(rtc.year))
					{
						rtc.mon++;
						rtc.day = 1;
					}
				}
				if (rtc.mon == 13)
				{
					rtc.mon = 1;
					rtc.year++;
				}
			}
		}
	}
}
#endif // RTC_FROM_TOSC

static uint8_t notLeap(uint16_t year)      //check for leap year
{
	if (!(year % 100))
	{
		return (uint8_t)(year % 400);
	}
	else
	{
		return (uint8_t)(year % 4);
	}
}

static uint8_t dayOfWeek(uint16_t d, const uint8_t m, uint16_t y)
{
	// I got this formula from https://stackoverflow.com/questions/6054016/c-program-to-find-day-of-week-given-date
	return (d += (m < 3 ? y-- : y - 2), 23*m/9 + (int)d + 4 + y/4- y/100 + y/400)%7; 
}

static void addTimeZoneOffset(struct Time *const dest, const struct Time *const src, const int8_t timeZone)
{
	*dest = *src; // Copy everything at first
	if(timeZone < 0) // Are we adding positive or negative time zone offset
	{
		if(-timeZone > (int8_t)src->hour) // We roll back to the previous day
		{
			dest->hour = 24 + timeZone + src->hour;
			if(src->weekday == 0) dest->weekday = 6;
			else dest->weekday = src->weekday - 1;
			dest->day = src->day - 1;
			if(src->day == 1) // Did we roll to previous month?
			{
				if(src->mon == 2 || src->mon == 4 || src->mon == 6 || src->mon == 8
					|| src->mon == 9 || src->mon == 11 || src->mon == 1)
				{
					dest->day = 31; // Previous month had 31 days
					dest->mon = src->mon - 1;
				}
				else if(src->mon == 5 || src->mon == 7 || src->mon == 10 || src->mon == 12)
				{
					dest->day = 30; // Previous month had 30 days
					dest->mon = src->mon - 1;
				}
				else if(notLeap(src->year)) // Previous month is February
				{
					dest->day = 28; // Previous month had 28 days
					dest->mon = src->mon - 1;
				}
				else 
				{
					dest->day = 29; // Previous month had 29 days
					dest->mon = src->mon - 1;
				}
				if(dest->mon == 0)
				{
					dest->year = src->year - 1;
				}
			}
		}
		else // We stay in current day
		{
			dest->hour = src->hour + timeZone;
		}
	}
	else // Adding positive time zone offset
	{
		uint8_t temp = src->hour + timeZone;
		dest->hour = temp % 24;
		if(temp > 23)
		{
			dest->day = src->day + 1;
			dest->weekday = (src->weekday + 1) % 7;
			if (dest->day == 32)
			{
				dest->mon = src->mon + 1;
				dest->day = 1;
			}
			else if (dest->day == 31)
			{
				if ((src->mon == 4) || (src->mon == 6) || (src->mon == 9) || (src->mon == 11))
				{
					dest->mon = src->mon + 1;
					dest->day = 1;
				}
			}
			else if (dest->day == 30)
			{
				if(src->mon == 2)
				{
					dest->mon = src->mon + 1;
					dest->day = 1;
				}
			}
			else if (dest->day == 29)
			{
				if((src->mon == 2) && notLeap(src->year))
				{
					dest->mon = src->mon + 1;
					dest->day = 1;
				}
			}
			if (dest->mon == 13)
			{
				dest->mon = 1;
				dest->year = src->year + 1;
			}
		}
	}
}

#ifdef USE_UNIX_TIME
static uint64_t calcUnix(const struct Time *const restrict time)
{
	const uint8_t dayMonth[][12] = {{31,28,31,30,31,30,31,31,30,31,30,31},
							 	    {31,29,31,30,31,30,31,31,30,31,30,31}};
	uint64_t unix = 0;
	if(notLeap(time->year)) // Calculate the seconds in the months up to this one
	{
		for(uint8_t i = 0; i < time->mon - 1; i++)
			unix += dayMonth[0][i] * 86400UL;
	}
	else
	{
		for(uint8_t i = 0; i < time->mon - 1; i++)
			unix += dayMonth[1][i] * 86400UL;
	}
	unix += time->day * 86400UL + time->hour * 3600UL + time->min * 60UL + time->sec; // Add in the seconds within the current month
	for(uint16_t i = 2020; i < time->year; i++) // Add in the seconds in the years from 2020 till the start of the current year
	{
		if(notLeap(i)) 
			unix += 3600UL * 24UL * 365UL;
		else
			unix += 3600UL * 24UL * 366UL;
	}
	return unix + 1577750400UL; // Add in seconds from 1970 to to Dec 31 2019
}
#endif

