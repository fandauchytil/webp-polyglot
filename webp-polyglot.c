#if 0
bin="${0%.c}"; if [ "$0" -nt "$bin" ]; then gcc -Wall -Wno-long-long -Wno-variadic-macros -Wno-unused-variable -Wno-unused-but-set-variable -std=gnu99 -ggdb3 -g3 -O0 "$0" -o "$bin" || exit $?; fi
if [ -n "$s" ]; then o="/tmp/strace.$(date +%F_%T)"; echo "STRACE OUTPUT: $o"; s="strace -f -o $o -yy -tt -s 512 "; fi
exec $s "$bin" "$@"; exit $?;
#else
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

typedef int8_t s8; typedef int16_t s16; typedef int32_t s32; typedef int64_t s64; typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
#define p(s...)  fprintf (stderr, s)
#define a(x)     ((void) ((x) && (errx(1, "%s %d %s(): if (%s)  -> [errno=%d] %s", __FILE__, __LINE__, __func__, #x, errno, strerror(errno)), 0) ))  // *NEGOVANY* assert()!
#define sx2i(s)  strtoll ((const char *) (s), NULL, 16)     // str to hex
#define s2i(s)   strtol  ((const char *) (s), NULL, 10)     // str to decimal
#define hitme()  do { int i; p ("Waiting for <ENTER> (PID=%d) ...", getpid()); read (0, &i, 1); } while(0)
#define p2(s...)  do { int i; struct tm *lt; static char buf[80]; static struct timeval tv; gettimeofday (&tv, NULL); lt = localtime (&(tv.tv_sec)); strftime (buf, sizeof (buf), "%F %T", lt); i = fprintf (stderr, "%s.%06ld (%ld.%ld)", buf, tv.tv_usec, tv.tv_sec - tv_last_cmd.tv_sec, tv.tv_usec - tv_last_cmd.tv_usec); fprintf (stderr, "%*c", 50-i, ' '); fprintf (stderr, s); tv_last_cmd = tv; } while (0)
void px (u8 *s, int n) { int i, j = 0; char b1[16*3+1+1], b2[16+1+1]; char *p1, *p2; long addr = 0; p ("-- %p --\n        0  1  2  3  4  5  6  7   8  9  A  B  C  D  E  F    01234567 89ABCDEF\n", s); while (j < n) { p1 = b1; p2 = b2; for (i = 0; i < 16 && j < n; i++, j++) { if (i == 8) { *p1 = *p2 = ' '; p1++; p2++; } p1 += sprintf (p1, "%02X " , s[j]); *p2 = s[j] >= ' ' && s[j] <= '~' ? s[j] : '.'; p2++; } if (i < 16) p1 += sprintf (p1, "%*c" , (16-i)*3 + (i<=8?1:0) , ' '); *p1 = *p2 = '\0'; p ("%04lX:  %s   %s\n", addr, b1, b2); addr += i; } p ("\n"); }


#if 0
ssize_t write_chunk (struct riff_chunk *ck, int fd_out, uint32_t out_off)
{
    int padding = 0;
    uint8_t *data = ck->chunk_data;

    p ("    -> writting id '%08x'   @  0x%x\n", ck->chunk_id, out_off);
    out_off += pwrite (fd_out, &(ck->chunk_id), sizeof (ck->chunk_id), out_off);
    p ("    -> writting size '%08x' @  0x%x\n", ck->chunk_size, out_off);
    out_off += pwrite (fd_out, &(ck->chunk_size), sizeof (ck->chunk_size), out_off);
    p ("    -> writting data            @  0x%x\n", out_off);
    //...

    if (ck->chunk_size % 2 != 0)
        padding = 1;

    size_t size = ck->chunk_size + padding;
    while (1)
    {
        ssize_t n = pwrite (fd_out, data, size, out_off);
        if (n <= 0)
            break;
        data += n;
        out_off += n;
        size -= n;
    }
    return out_off;
}
#endif

struct riff_chunk {
    uint32_t  chunk_id;            // Chunk type identifier
    uint32_t  chunk_size;          // Chunk size field (size of ck_data)
    uint8_t   chunk_data[]; // Chunk data
};

#define SIZEOF_CHUNK_ID         ((uint32_t) sizeof (((struct riff_chunk *) 0)->chunk_id))
#define SIZEOF_CHUNK_SIZE       ((uint32_t) sizeof (((struct riff_chunk *) 0)->chunk_size))
#define SIZEOF_CHUNK_HEADER     ((uint32_t) (SIZEOF_CHUNK_ID + SIZEOF_CHUNK_SIZE))


#define pad(i)      ((i) + (((i) & 1) ? 1 : 0))

int main (int argc, char *argv[])
{
    if (argc < 2)
    {
        p ("Usage: %s <RIFF_FILE> [<DATA_FILE> <OUT_FILE>]\n", argv[0]);
        p ("Examples:\n");
        p ("    %s image.webp                       # show info and exit\n", argv[0]);
        p ("    %s image.webp mbr.bin out.webp      # create 'out.webp' containing 'mbr.bin' and a image from 'image.webp'\n", argv[0]);
        exit (1);
    }

    int fd; a ((fd = open (argv[1], O_RDONLY)) < 0);

    struct stat sb; a (fstat (fd, &sb) == -1);
    uint8_t *file; a ((file = mmap (NULL, sb.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0)) == MAP_FAILED);

    struct riff_chunk *riff;
    struct riff_chunk *vp8l;
    struct riff_chunk *ck;
    struct riff_chunk tmp;

    uint32_t canvas = 0;
    uint32_t vp8l_image_width = 0;
    uint32_t vp8l_image_height = 0;


    uint32_t in_off;
    uint32_t out_off = 0;

    riff = (struct riff_chunk *) file;

    if (riff->chunk_id != 0x46464952)  // "RIFF"
    {
        p ("ERROR: Not a RIFF container!\n");
        exit (1);
    }

    p ("RIFF container detected:\n");
    p ("    chunk id:           0x%08x\n", riff->chunk_id);
    p ("    chunk size:         0x%08x\n", riff->chunk_size);
    p ("    file size:          0x%08lx\n", sb.st_size);
    p ("        file sz - hdr:  0x%08lx\n", sb.st_size - SIZEOF_CHUNK_HEADER);

    if (riff->chunk_size & 1)
        p ("!!! ERROR: RIFF chunk size is ODD! Must be EVEN !!!\n");
    else if ((riff->chunk_size + SIZEOF_CHUNK_HEADER) != sb.st_size)
        p ("WARNING: Potentially bad file size!\n");


    if (*(uint32_t *) riff->chunk_data == 0x50424557)    /* "WebP" */
        p ("\nWEBP RIFF container detected:\n");
    else
        p ("ERROR: Unknown RIFF format!\n");


    /* sizes of: RIFF_ID + RIFF_SIZE + WEBP_ID = 4 + 4 + 4 = 0x0c */
    in_off = SIZEOF_CHUNK_ID + SIZEOF_CHUNK_SIZE + SIZEOF_CHUNK_ID;

    while (in_off < riff->chunk_size)
    {
        p ("\n    [idx: 0x%04x]\n", in_off);

        ck = (struct riff_chunk *) (file + in_off);

        p ("    \"%c%c%c%c\" (0x%08x ; \"%02x%02x%02x%02x\")\n", ((char *)&ck->chunk_id)[0], ((char *) &ck->chunk_id)[1], ((char*)&ck->chunk_id)[2], ((char*)&ck->chunk_id)[3], ck->chunk_id, ((char *)&ck->chunk_id)[0], ((char *) &ck->chunk_id)[1], ((char*)&ck->chunk_id)[2], ((char*)&ck->chunk_id)[3]);
        p ("        size:         0x%08x\n", ck->chunk_size);
        p ("        offset:       0x%08x\n", in_off);

        if (ck->chunk_id == 0x58385056) /* VP8X -- extended header */
        {
            p ("        (extended header)\n");
            p ("        flags: %02x\n", ck->chunk_data[0]);
            p ("            ICC profile:    %d\n", (ck->chunk_data[0] >> 5) & 1);
            p ("            Alpha:          %d\n", (ck->chunk_data[0] >> 4) & 1);
            p ("            Exif metadata:  %d\n", (ck->chunk_data[0] >> 3) & 1);
            p ("            XMP metadata:   %d\n", (ck->chunk_data[0] >> 2) & 1);
            p ("            Animation:      %d\n", (ck->chunk_data[0] >> 1) & 1);

            canvas = (ck->chunk_data[6] << 16) | (ck->chunk_data[5] << 8) | ck->chunk_data[4];
            p ("        Canvas width:       0x%06x +1 = 0x%06x (decimal = %d)\n", canvas, canvas+1, canvas+1);
            canvas = (ck->chunk_data[9] << 16) | (ck->chunk_data[8] << 8) | ck->chunk_data[7];
            p ("        Canvas height:      0x%06x +1 = 0x%06x (decimal = %d)\n", canvas, canvas+1, canvas+1);
        }
        else if (ck->chunk_id == 0x4c385056) /* VP8L */
        {
            vp8l = ck;

            p ("        (lossless image data)\n");
            if (vp8l->chunk_data[0] != 0x2f)
                p ("ERROR: signature is not 0x2f!\n");
            p ("        signature: %02x\n", vp8l->chunk_data[0]);

            canvas = *(uint32_t *) &(vp8l->chunk_data[1]);
            vp8l_image_width  = canvas & 0x3fff;
            vp8l_image_height = (canvas >> 14) & 0x3fff;
            p ("        Canvas width:  0x%06x +1 = 0x%06x (decimal = %d)\n", vp8l_image_width,  vp8l_image_width+1,  vp8l_image_width+1 );
            p ("        Canvas height: 0x%06x +1 = 0x%06x (decimal = %d)\n", vp8l_image_height, vp8l_image_height+1, vp8l_image_height+1);
        }

        in_off += pad (ck->chunk_size) + SIZEOF_CHUNK_HEADER;
    }





    if (argc > 2)
    {
        int fd_out;
        static uint8_t buf[1024*1024*600];
        int n;
        //struct riff_chunk *riff;
        struct riff_chunk *vp8x;
        struct riff_chunk *vp8l_out;
        struct riff_chunk *my_data;

        a (vp8l == NULL);

        p ("\nCREATING NEW WEBP:\n");

        /*
         *                                           RIFF_SZ
         *                     .-------------------------------------------------------------------------------------------.
         *                     |            MBR_SZ                                                                         |
         *      v--------------v--------------------------------------------------v                                        v
         *      "RIFF" RIFF_SZ "WEBP" "VP8X" VP8X_SZ 10 "EXIF" EXIF_SZ EXIF_DATA_SZ EXIF_padding "VP8L" VP8L_SZ VP8L_DATA_SZ
         *      ^ 4 + 4 + 4 + 4 + 4 + 10 = 30 = 0x1e  ^              ^                           ^-------------------------^
         *      |-------------------------------------'              |                            4 + 4 + VP8L_DATA_SZ = 8 + VP8L_DATA_SZ
         *      | 30 + 4 + 4 = 38 = 0x26                             |
         *      '----------------------------------------------------'
         *
         * EXIF_DATA_SZ = my_asm_code_size - 0x26
         * EXIF_padding = RIFF_SZ - ((0x1e) + (8 + VP8L_DATA_SZ))
         *   (EXIF headers are ignored because it is the EXIF_SZ we are computing.)
         */
        a ((fd_out = open (argv[2], O_RDWR|O_CREAT|O_TRUNC, 0644)) < 0);

        riff = (struct riff_chunk *) (buf + 0);

        riff->chunk_id   = 0x46464952;      // "RIFF"
        riff->chunk_size = 0x00223c3c;            // "<<\"\0"  [2.14 MiB]
        riff->chunk_size = 0x00003c3c;            // "<<\0\0"  [2.14 MiB]
        //riff->chunk_size = 0x01223c3c;            // "<<\"\1"  <-- bash checks for \0 on the first line  [18.14 MiB]
        //riff->chunk_size = 0x21223c3c;            // "<<\"!"  <-- bash checks for \0 on the first line  [18.14 MiB]
        *((uint32_t *) &(riff->chunk_data)) = 0x50424557;    // "WEBP"

        vp8x = (struct riff_chunk *) (buf + SIZEOF_CHUNK_HEADER + SIZEOF_CHUNK_ID);

        vp8x->chunk_id   = 0x58385056;     // "VP8X"
        vp8x->chunk_size = 0x0a22;            // "\"\n"
        vp8x->chunk_size = 0x0a;            // "\"\n"
        vp8x->chunk_data[0] = 0x08;        // EXIF data
        //vp8x->chunk_data[0] = 0;
        vp8x->chunk_data[1] = 0;
        vp8x->chunk_data[2] = 0;
        vp8x->chunk_data[3] = 0;
        // image width (24 bits)
        vp8x->chunk_data[4] = vp8l_image_width & 0xff;
        vp8x->chunk_data[5] = (vp8l_image_width >> 8) & 0xff;
        vp8x->chunk_data[6] = (vp8l_image_width >> 16) & 0xff;
        // image height (24 bits)
        vp8x->chunk_data[7] = vp8l_image_height& 0xff;
        vp8x->chunk_data[8] = (vp8l_image_height >> 8) & 0xff;
        vp8x->chunk_data[9] = (vp8l_image_height >> 16) & 0xff;


        vp8l_out = (struct riff_chunk *) (((uint8_t *) vp8x) + pad (vp8x->chunk_size) + SIZEOF_CHUNK_HEADER);
        a (memmove (vp8l_out, vp8l, vp8l->chunk_size + SIZEOF_CHUNK_HEADER) != vp8l_out);


        /* pad section */
        struct riff_chunk *exif_pad;
        exif_pad = (struct riff_chunk *) (((uint8_t *) vp8l_out) + pad (vp8l_out->chunk_size) + SIZEOF_CHUNK_HEADER);
        exif_pad->chunk_id = 0x46495845; // "EXIF"
        exif_pad->chunk_size = riff->chunk_size - SIZEOF_CHUNK_ID - SIZEOF_CHUNK_HEADER - pad (vp8x->chunk_size) - SIZEOF_CHUNK_HEADER - pad (vp8l_out->chunk_size) - SIZEOF_CHUNK_HEADER - SIZEOF_CHUNK_HEADER;
        /* EXIF_DATA =                            WEBP_ID +         VP8X_ID + VP8X_SIZE + VP8X_DATA +              VP8L_ID + VP8L_SIZE + VP8L_DATA +                  EXIF_ID + EXIF_SIZE */

        p ("    paddings:\n");
        p ("        riff->chunk_size:       %x\n", riff->chunk_size);
        p ("        vp8x->chunk_size:       %x -> %x\n", vp8x->chunk_size, pad (vp8x->chunk_size));
        p ("        vp8l->chunk_size:       %x -> %x\n", vp8l->chunk_size, pad (vp8l->chunk_size));
        p ("        exif_pad->chunk_size:   %x -> %x\n", exif_pad->chunk_size, pad (exif_pad->chunk_size));

        

        void *data = buf;
        size_t size = riff->chunk_size + SIZEOF_CHUNK_HEADER;

        p ("    Writing size: %lx\n", size);

        while (1)
        {
            ssize_t n;
            a ((n = write (fd_out, data, size)) < 0);
            if (n == 0)
                break;
            data += n;
            size -= n;
        }
    }

    return 0;
}
#endif

