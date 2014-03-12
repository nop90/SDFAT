#ifndef MINLIB_H
#define MINLIB_H

#define TICKS_PER_SEC 0x80C0000
#define TICKS_PER_MSEC (TICKS_PER_SEC/1000)
#define TICKS_PER_USEC (TICKS_PER_MSEC/1000)

unsigned long long GetSystemTick(void);
void nsleep(unsigned long long ns);
void usleep(unsigned long long us);
void msleep(unsigned long long ms);
void sleep(unsigned long long s);

unsigned long long __aeabi_uidiv (unsigned long long numerator, unsigned long long denominator);

 
#endif

