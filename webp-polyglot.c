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

#define ACT_SHOW    1
#define ACT_MBR     2
#define ACT_CHNG    4


#define CHUNK_ID_RIFF       0x46464952
#define CHUNK_ID_WEBP       0x50424557
#define CHUNK_ID_VP8L       0x4c385056
#define CHUNK_ID_VP8X       0x58385056


uint8_t *find_chunk_id (uint8_t *file, uint32_t id)
{
    uint32_t in_off = 0;
    uint32_t in_off_next;
    struct riff_chunk *ck, *riff;

    riff = (struct riff_chunk *) (file + in_off);

    if (riff->chunk_id != CHUNK_ID_RIFF)
        return NULL;

    in_off += SIZEOF_CHUNK_ID + SIZEOF_CHUNK_SIZE + SIZEOF_CHUNK_ID;

    while (in_off < riff->chunk_size)
    {
        ck = (struct riff_chunk *) (file + in_off);

#if 0
        in_off_next = in_off + pad (ck->chunk_size) + SIZEOF_CHUNK_HEADER;

        p ("\n    [idx: 0x%04x  ->  next idx: 0x%04x (remains: 0x%04x)]\n", in_off, in_off_next, file_size - in_off_next);

        p ("    \"%c%c%c%c\" (0x%08x ; \"%02x%02x%02x%02x\")\n", ((char *)&ck->chunk_id)[0], ((char *) &ck->chunk_id)[1], ((char*)&ck->chunk_id)[2], ((char*)&ck->chunk_id)[3], ck->chunk_id, ((char *)&ck->chunk_id)[0], ((char *) &ck->chunk_id)[1], ((char*)&ck->chunk_id)[2], ((char*)&ck->chunk_id)[3]);
        p ("        size:         0x%08x\n", ck->chunk_size);
        //p ("        offset:       0x%08x\n", in_off);
#endif

        if (ck->chunk_id == id)
            return file + in_off;

        in_off += pad (ck->chunk_size) + SIZEOF_CHUNK_HEADER;
    }
    return NULL;
}

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

    int action = ACT_SHOW;

    if (argv[1][0] == '-')
    {
        switch (argv[1][1])
        {
            case 'm':
                action = ACT_MBR;
                break;
            case 'c':
                action = ACT_CHNG;
                break;
        }
    }

    int fd;

    struct stat sb;
    uint8_t *file, *file_ptr;

    struct riff_chunk *riff;
    struct riff_chunk *vp8l;
    struct riff_chunk *ck;
    struct riff_chunk tmp;

    uint32_t canvas = 0;
    uint32_t vp8l_image_width = 0;
    uint32_t vp8l_image_height = 0;


    uint32_t in_off;
    uint32_t out_off = 0;


    if (action & ACT_SHOW)
    {

    a ((fd = open (argv[1], O_RDONLY)) < 0);
    a (fstat (fd, &sb) == -1);
    a ((file = mmap (NULL, sb.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0)) == MAP_FAILED);

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


    if (*(uint32_t *) riff->chunk_data == CHUNK_ID_WEBP)
        p ("\nWEBP RIFF container detected:\n");
    else
        p ("ERROR: Unknown RIFF format!\n");


    /* sizes of: RIFF_ID + RIFF_SIZE + WEBP_ID = 4 + 4 + 4 = 0x0c */
    in_off = SIZEOF_CHUNK_ID + SIZEOF_CHUNK_SIZE + SIZEOF_CHUNK_ID;

    while (in_off < riff->chunk_size)
    {
        ck = (struct riff_chunk *) (file + in_off);

        uint32_t in_off_next = in_off + pad (ck->chunk_size) + SIZEOF_CHUNK_HEADER;

        p ("\n    [idx: 0x%04x  ->  next idx: 0x%04x (remains: 0x%04x)]\n", in_off, in_off_next, riff->chunk_size - in_off_next);

        p ("    \"%c%c%c%c\" (0x%08x ; \"%02x%02x%02x%02x\")\n", ((char *)&ck->chunk_id)[0], ((char *) &ck->chunk_id)[1], ((char*)&ck->chunk_id)[2], ((char*)&ck->chunk_id)[3], ck->chunk_id, ((char *)&ck->chunk_id)[0], ((char *) &ck->chunk_id)[1], ((char*)&ck->chunk_id)[2], ((char*)&ck->chunk_id)[3]);
        p ("        size:         0x%08x\n", ck->chunk_size);
        p ("        offset:       0x%08x\n", in_off);

        if (ck->chunk_id == CHUNK_ID_VP8X) /* VP8X -- extended header */
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
        else if (ck->chunk_id == CHUNK_ID_VP8L)
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
    }




    if (action & ACT_CHNG)
    {
        uint8_t *file1, *file2, *script_data;
        int fd_file1, fd_file2, fd_script, fd_out;
        static uint8_t buf[1024*1024*16];
        //struct riff_chunk *riff;
        struct riff_chunk *vp8x;
        struct riff_chunk *img1_vp8l_out, *img2_vp8l_out;
        struct riff_chunk *my_data;
        struct riff_chunk *img1_vp8l, *img2_vp8l;
        size_t script_data_len;


        a ((fd_file1 = open (argv[2], O_RDONLY, 0644)) < 0);
        a ((fd_file2 = open (argv[3], O_RDONLY, 0644)) < 0);
        a ((fd_script = open (argv[4], O_RDONLY, 0644)) < 0);
        a ((fd_out = open (argv[5], O_RDWR|O_CREAT|O_TRUNC, 0755)) < 0);


        a (fstat (fd_file1, &sb) == -1);
        a ((file1 = mmap (NULL, sb.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd_file1, 0)) == MAP_FAILED);
        a (fstat (fd_file2, &sb) == -1);
        a ((file2 = mmap (NULL, sb.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd_file2, 0)) == MAP_FAILED);
        a (fstat (fd_script, &sb) == -1);
        script_data_len = sb.st_size;
        a ((script_data = mmap (NULL, script_data_len, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd_script, 0)) == MAP_FAILED);

        a ((img1_vp8l = (struct riff_chunk *) find_chunk_id (file1, CHUNK_ID_VP8L)) == NULL);
        a ((img2_vp8l = (struct riff_chunk *) find_chunk_id (file2, CHUNK_ID_VP8L)) == NULL);

        canvas = *(uint32_t *) &(img1_vp8l->chunk_data[1]);
        uint32_t img1_vp8l_image_width  = canvas & 0x3fff;
        uint32_t img1_vp8l_image_height = (canvas >> 14) & 0x3fff;

        canvas = *(uint32_t *) &(img2_vp8l->chunk_data[1]);
        uint32_t img2_vp8l_image_width  = canvas & 0x3fff;
        uint32_t img2_vp8l_image_height = (canvas >> 14) & 0x3fff;

        p ("img1: %d x %d\n", img1_vp8l_image_width, img1_vp8l_image_height);
        p ("img2: %d x %d\n", img2_vp8l_image_width, img2_vp8l_image_height);

        a (img1_vp8l_image_width != img2_vp8l_image_width);
        a (img1_vp8l_image_height != img2_vp8l_image_height);


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

        riff = (struct riff_chunk *) (buf + 0);

        riff->chunk_id = CHUNK_ID_RIFF;
        riff->chunk_size = 0x0023230a;        // "\n##\0"

        *(uint32_t *) (riff->chunk_data) = CHUNK_ID_WEBP;

        vp8x = (struct riff_chunk *) (buf + SIZEOF_CHUNK_HEADER + SIZEOF_CHUNK_ID);

        vp8x->chunk_id   = CHUNK_ID_VP8X;
        vp8x->chunk_size = 0x0a;
        //ck->chunk_data[0] = 0x08;        // EXIF data
        vp8x->chunk_data[0] = 0;
        //vp8x->chunk_data[1] = 0;
        //vp8x->chunk_data[2] = 0;
        vp8x->chunk_data[1] = '\n';      // XXX
        vp8x->chunk_data[2] = '#';      // XXX
        vp8x->chunk_data[3] = 0;
        // image width (24 bits)
        vp8x->chunk_data[4] = img1_vp8l_image_width & 0xff;
        vp8x->chunk_data[5] = (img1_vp8l_image_width >> 8) & 0xff;
        vp8x->chunk_data[6] = (img1_vp8l_image_width >> 16) & 0xff;
        // image height (24 bits)
        vp8x->chunk_data[7] = img1_vp8l_image_height& 0xff;
        vp8x->chunk_data[8] = (img1_vp8l_image_height >> 8) & 0xff;
        vp8x->chunk_data[9] = (img1_vp8l_image_height >> 16) & 0xff;

        //px (vp8x, vp8x->chunk_size + SIZEOF_CHUNK_HEADER);
        //px (buf, SIZEOF_CHUNK_HEADER + SIZEOF_CHUNK_ID + vp8x->chunk_size + SIZEOF_CHUNK_HEADER);

        my_data = (struct riff_chunk *) (((uint8_t *) vp8x) + pad (vp8x->chunk_size) + SIZEOF_CHUNK_HEADER);

        my_data->chunk_id = 0x4c454853; // "SHEL"
        my_data->chunk_size = riff->chunk_size - (0x1e + SIZEOF_CHUNK_HEADER + pad (img1_vp8l->chunk_size) + SIZEOF_CHUNK_HEADER + pad (img2_vp8l->chunk_size));

        p ("    paddings:\n");
        p ("        riff->chunk_size:       %x\n", riff->chunk_size);
        p ("        mbr.bin size+hdrs:      %lx\n", sb.st_size);
        p ("        WEBP hdrs:              %x\n", 0x1e);
        p ("        vp8x->chunk_size:       %x -> %x\n", vp8x->chunk_size, pad (vp8x->chunk_size));
        p ("        vp8l headers:           %x\n",  SIZEOF_CHUNK_HEADER);
        p ("        vp8l_01->chunk_size:    %x -> %x\n", img1_vp8l->chunk_size, pad (img1_vp8l->chunk_size));
        p ("        vp8l_02->chunk_size:    %x -> %x\n", img2_vp8l->chunk_size, pad (img2_vp8l->chunk_size));
        p ("        (skiping exif hdrs:     %x)\n", SIZEOF_CHUNK_HEADER);
        p ("        my_data->chunk_size:    %x -> %x\n", my_data->chunk_size, pad (my_data->chunk_size));
        p ("        my_data size:           %x\n", my_data->chunk_size);


        img1_vp8l_out = (struct riff_chunk *) (((uint8_t *) my_data) + pad (my_data->chunk_size) + SIZEOF_CHUNK_HEADER);
        a (memmove (img1_vp8l_out, img1_vp8l, img1_vp8l->chunk_size + SIZEOF_CHUNK_HEADER) != img1_vp8l_out);

        img2_vp8l_out = (struct riff_chunk *) (((uint8_t *) img1_vp8l_out) + pad (img1_vp8l_out->chunk_size) + SIZEOF_CHUNK_HEADER);
        a (memmove (img2_vp8l_out, img2_vp8l, img2_vp8l->chunk_size + SIZEOF_CHUNK_HEADER) != img2_vp8l_out);
        img2_vp8l_out->chunk_id = CHUNK_ID_VP8L - 1;


        char *ptr = (char *) my_data->chunk_data;

        ptr[0] = '\n';
        ptr++;

        ptr += snprintf (ptr, 128, "IMG1=%d; IMG2=%d;\n", (u32) ((void *)img1_vp8l_out - (void*) buf),  (u32) ((void *)img2_vp8l_out - (void*) buf));
        a (memmove (ptr, script_data, script_data_len) != ptr);


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


#if 0
    if (action & ACT_MBR)
    {
        int fd_mbr, fd_out;
        static uint8_t buf[1024*1024*16];
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


        a ((fd_mbr = open (argv[2], O_RDONLY, 0644)) < 0);
        a ((fd_out = open (argv[3], O_RDWR|O_CREAT|O_TRUNC, 0644)) < 0);

        a (fstat (fd_mbr, &sb) == -1);

        a ((n = read (fd_mbr, buf, sb.st_size)) != sb.st_size);

        riff = (struct riff_chunk *) (buf + 0);
        vp8x = (struct riff_chunk *) (buf + SIZEOF_CHUNK_HEADER + SIZEOF_CHUNK_ID);

        vp8x->chunk_id   = CHUNK_ID_VP8X;
        vp8x->chunk_size = 0x0a;
        //ck->chunk_data[0] = 0x08;        // EXIF data
        vp8x->chunk_data[0] = 0;
        vp8x->chunk_data[1] = 0;
        vp8x->chunk_data[2] = 0;
        vp8x->chunk_data[3] = 0;
        //XXX
        vp8x->chunk_data[1] = '\n';      // XXX
        vp8x->chunk_data[2] = '#';      // XXX
        //XXX
        vp8x->chunk_data[1] = '\'';      // XXX
        vp8x->chunk_data[2] = '\n';      // XXX
        //XXX
        // image width (24 bits)
        vp8x->chunk_data[4] = vp8l_image_width & 0xff;
        vp8x->chunk_data[5] = (vp8l_image_width >> 8) & 0xff;
        vp8x->chunk_data[6] = (vp8l_image_width >> 16) & 0xff;
        // image height (24 bits)
        vp8x->chunk_data[7] = vp8l_image_height& 0xff;
        vp8x->chunk_data[8] = (vp8l_image_height >> 8) & 0xff;
        vp8x->chunk_data[9] = (vp8l_image_height >> 16) & 0xff;

        //px (vp8x, vp8x->chunk_size + SIZEOF_CHUNK_HEADER);
        //px (buf, SIZEOF_CHUNK_HEADER + SIZEOF_CHUNK_ID + vp8x->chunk_size + SIZEOF_CHUNK_HEADER);

        my_data = (struct riff_chunk *) (((uint8_t *) vp8x) + pad (vp8x->chunk_size) + SIZEOF_CHUNK_HEADER);

        my_data->chunk_size = riff->chunk_size - (0x1e + SIZEOF_CHUNK_HEADER + pad (vp8l->chunk_size));

        p ("    paddings:\n");
        p ("        riff->chunk_size:       %x\n", riff->chunk_size);
        p ("        mbr.bin size+hdrs:      %lx\n", sb.st_size);
        p ("        WEBP hdrs:              %x\n", 0x1e);
        p ("        vp8x->chunk_size:       %x -> %x\n", vp8x->chunk_size, pad (vp8x->chunk_size));
        p ("        vp8l headers:           %x\n",  SIZEOF_CHUNK_HEADER);
        p ("        vp8l->chunk_size:       %x -> %x\n", vp8l->chunk_size, pad (vp8l->chunk_size));
        p ("        (skiping exif hdrs:     %x)\n", SIZEOF_CHUNK_HEADER);
        p ("        my_data->chunk_size:    %x -> %x\n", my_data->chunk_size, pad (my_data->chunk_size));
        p ("        my_data size:           %x\n", riff->chunk_size - (0x1e + SIZEOF_CHUNK_HEADER + pad (vp8l->chunk_size)));

        vp8l_out = (struct riff_chunk *) (((uint8_t *) my_data) + pad (my_data->chunk_size) + SIZEOF_CHUNK_HEADER);
        //px (my_data, my_data->chunk_size + SIZEOF_CHUNK_HEADER + SIZEOF_CHUNK_HEADER);
        a (memmove (vp8l_out, vp8l, vp8l->chunk_size + SIZEOF_CHUNK_HEADER) != vp8l_out);


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
#endif

    return 0;
}
#endif

