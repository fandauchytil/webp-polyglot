#ifndef _HACKING__H_
#define _HACKING__H_

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <errno.h>
#include <err.h>
#include <stddef.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sched.h>
#include <signal.h>
#include <sys/mount.h>
#include <syscall.h>
#include <pthread.h>
#include <byteswap.h>


typedef int8_t      s8;
typedef int16_t     s16;
typedef int32_t     s32;
typedef int64_t     s64;
typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;

#define p(s...)  fprintf (stderr, s)

#define a(x)     ((void) ((x) && (errx (1, "%s %d %s(): if (%s)  -> [errno=%d] %s", __FILE__, __LINE__, __func__, #x, errno, strerror (errno)), 0) ))  // *NEGOVANY* assert()!

#define sx2i(s)  strtoll ((const char *) (s), NULL, 16)     // str hex num -> num
#define s2i(s)   strtol  ((const char *) (s), NULL, 10)     // str decimal num -> num

#define hitme()  do { int i; p ("Waiting for <ENTER> (PID=%d) ...", getpid()); read (0, &i, 1); } while(0)

#define p2(s...)  do { int i; struct tm *lt; static char buf[80]; static struct timeval tv; gettimeofday (&tv, NULL); lt = localtime (&(tv.tv_sec)); strftime (buf, sizeof (buf), "%F %T", lt); i = fprintf (stderr, "%s.%06ld (%ld.%ld)", buf, tv.tv_usec, tv.tv_sec - tv_last_cmd.tv_sec, tv.tv_usec - tv_last_cmd.tv_usec); fprintf (stderr, "%*c", 50-i, ' '); fprintf (stderr, s); tv_last_cmd = tv; } while (0)

// String to hex-string
void sxs (char *buf, unsigned char *s, int n)
{
    int i = 0;
    //static char b[1024];

    for (i = 0; i < n; i++) {
        sprintf (buf+(i*3), "%02X ", s[i]);
    }
    if (i == 0)
        buf[i] = '\0';
    else
        buf[i*3 -1] = '\0';
}

// Hex-string to bin
int sx2b (char *str, char *out_buf)
{
    int i, i2, c = 0, num;
    char *out = (out_buf == NULL) ? str : out_buf;

    for (i = i2 = 0; str[i] != '\0'; i++)
    {
        if ((str[i] >= 'a' && str[i] <= 'f'))
            num = str[i] - 'a' + 0x0a;
        else if ((str[i] >= 'A' && str[i] <= 'F'))
            num = str[i] - 'A' + 0x0a;
        else if ((str[i] >= '0' && str[i] <= '9'))
            num = str[i] - '0';
        else
            continue;

        if (c == 0) {
            out[i2] = num << 4;
            c = 1;
        }
        else {
            out[i2] = num | out[i2];
            i2++;
            c = 0;
        }
    }
    return i2;
}

#define px(s, n)    (pxd ((u8 *) (s), (size_t) (n)))
void pxd (u8 *s, size_t n)
{
    int i, j = 0;
    char b1[16*3+1+1], b2[16+1+1];
    char *p1, *p2;
    long addr = 0;

    p ("-- %p --\n        0  1  2  3  4  5  6  7   8  9  A  B  C  D  E  F    01234567 89ABCDEF\n", s);

    while (j < n)
    {
        p1 = b1;
        p2 = b2;

        for (i = 0; i < 16 && j < n; i++, j++)
        {
            if (i == 8)
            {
                *p1 = *p2 = ' ';
                p1++;
                p2++;
            }
            p1 += sprintf (p1, "%02X " , s[j]);
            *p2 = s[j] >= ' ' && s[j] <= '~' ? s[j] : '.';
            p2++;
        }
        if (i < 16)
            p1 += sprintf (p1, "%*c" , (16-i)*3 + (i<=8?1:0) , ' ');
        *p1 = *p2 = '\0';
        p ("%04lX:  %s   %s\n", addr, b1, b2);
        addr += i;
    }
    p ("\n");
}

#endif /* _HACKING__H_ */

