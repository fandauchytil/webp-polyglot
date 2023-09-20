//bin/true; bin="${0%.c}"; if [ "$0" -nt "$bin" ]; then gcc -Wall -Wno-long-long -Wno-variadic-macros -Wno-unused-variable -Wno-unused-but-set-variable -I ~/include -std=gnu99 -ggdb3 -g3 -O0 "$0" -o "$bin" || exit $?; fi; if [ -n "$s" ]; then o="/tmp/strace.$(date +%F_%T)"; echo "STRACE OUTPUT: $o"; s="strace -f -o $o -yy -tt -s 512 "; fi; exec $s "$bin" "$@"; exit $?;
#include "hacking.h"


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

void parse_webp (char *filename)
{
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


    p ("== %s\n\n", filename);

    a ((fd = open (filename, O_RDONLY)) < 0);
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

    p ("\n\n");

    close (fd);
    munmap (file, sb.st_size);
}


void usage (char *name, int status)
{
    p ("Usage: %s <RIFF_FILE> [<DATA_FILE> <OUT_FILE>]\n", name);
    p ("\n");
    p ("Examples:\n");
    p ("    %s image.webp                       # show info and exit\n", name);
    p ("    %s -S 20200000 -s script-ruby-equal.rb -p -1 $'\';\n' -C 203d2700 -c -o out.webp cat.webp dog.webp\n", name);
    p ("    %s -s mbr.bin -c -o out.webp h4x_16x16-vp8l-lossless.webp      # create 'out.webp' containing 'mbr.bin' and a image from 'image.webp'\n", name);
    exit (status);
}

uint8_t *insert_chunk (uint8_t *buf, uint8_t *data, uint32_t data_len, uint32_t id)
{
    struct riff_chunk *ck = (struct riff_chunk *) buf;

    ck->chunk_id = id;
    ck->chunk_size = data_len;

    buf += SIZEOF_CHUNK_HEADER;

    if (data)
        if (memmove (buf, data, data_len) != buf)
            return NULL;

    return buf + pad (data_len);
}


int main (int argc, char *argv[])
{
    char *opt__out_filename = NULL, *opt__script_filename = NULL;
    int action = ACT_SHOW;
    int i, opt;
    uint32_t opt__riff_chunk_size = 0x00000000;
    //uint32_t opt__riff_chunk_size = 0x0023230a;         // "\n##\0"  <-- shell specific
    uint32_t opt__vp8x_chunk_data = 0x00000000;
    uint32_t opt__script_chunk_id = 0x4b434148;         // "HACK"
    uint32_t opt__script_chunk_size = 0x00000000;
    int opt__script_has_riff_header = 0;
    int opt__script_data_img_pos    = 0;
    int script_data_first_len = 0;
    char *opt__script_data_first = NULL;


    while ((opt = getopt (argc, argv, "0:1:C:S:o:s:pHmch")) != -1)
    {
        switch (opt)
        {
            case '0':
                opt__vp8x_chunk_data = bswap_32 (strtoul ((const char *) (optarg), NULL, 16));
                break;
            case '1':
                opt__script_data_first = optarg;
                break;
            case 'C':
                opt__riff_chunk_size = bswap_32 (strtoul ((const char *) (optarg), NULL, 16));
                break;
            case 'S':
                opt__script_chunk_size = bswap_32 (strtoul ((const char *) (optarg), NULL, 16));
                break;
            case 'o':
                opt__out_filename = optarg;
                break;
            case 's':
                opt__script_filename = optarg;
                break;
            case 'p':
                opt__script_data_img_pos = 1;
                break;
            case 'H':
                opt__script_has_riff_header = 1;
                break;
            case 'm':
                action = ACT_MBR;
                break;
            case 'c':
                action = ACT_CHNG;
                break;
            case 'h':   /* Help */
                usage (argv[0], EXIT_SUCCESS);
            case ':':   /* Chybi hodnota parametru */
            case '?':   /* Neznama opsna */
            default:
                usage (argv[0], EXIT_FAILURE);
        }
    }

    if (optind >= argc)
        usage (argv[0], EXIT_FAILURE);

    if (opt__script_data_first)
        script_data_first_len = strlen (opt__script_data_first);


    if (action & ACT_SHOW)
    {
        for (i = optind; i < argc; i++)
            parse_webp (argv[i]);
    }

    if (action & ACT_CHNG)
    {
        struct stat sb;
        uint8_t *script_data = NULL;
        int num_images = 0, fd, fd_script, fd_out;
        static uint8_t buf[1024*1024*16];
        struct riff_chunk *riff;
        struct riff_chunk *my_data;
        struct riff_chunk *padding;
        size_t script_data_len;

        uint32_t canvas;

        a (opt__out_filename == NULL);
        a ((fd_out = open (opt__out_filename, O_RDWR|O_CREAT|O_TRUNC, 0755)) < 0);

        if (opt__script_filename)
        {
            a ((fd_script = open (opt__script_filename, O_RDONLY, 0644)) < 0);
            a (fstat (fd_script, &sb) == -1);
            script_data_len = sb.st_size;
            a ((script_data = mmap (NULL, script_data_len, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd_script, 0)) == MAP_FAILED);
        }

        struct img_files {
            uint8_t *data;
            struct riff_chunk *vp8l;
            uint32_t offset;
            uint32_t vp8l_image_width;
            uint32_t vp8l_image_height;
            struct img_files *next;
        };

        struct img_files *img_first = NULL, *img = NULL, *img_last = NULL;

        for (i = optind; i < argc; i++)
        {
            a ((fd = open (argv[i], O_RDONLY, 0644)) < 0);
            a (fstat (fd, &sb) == -1);

            a ((img = (struct img_files *) malloc (sizeof (struct img_files))) == NULL);

            img->next = NULL;

            if (img_first == NULL)
                img_first = img;

            a ((img->data = mmap (NULL, sb.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0)) == MAP_FAILED);
            close (fd);

            a ((img->vp8l = (struct riff_chunk *) find_chunk_id (img->data, CHUNK_ID_VP8L)) == NULL);

            canvas = *(uint32_t *) &(img->vp8l->chunk_data[1]);
            img->vp8l_image_width  = canvas & 0x3fff;
            img->vp8l_image_height = (canvas >> 14) & 0x3fff;

            p ("img %d: %d x %d\n", num_images, img->vp8l_image_width, img->vp8l_image_height);

            a (img->vp8l_image_width != img_first->vp8l_image_width);
            a (img->vp8l_image_height != img_first->vp8l_image_height);

            if (img_last != NULL)
                img_last->next = img;

            img_last = img;

            num_images++;
        }


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

        uint8_t *ptr_data;

        ptr_data = buf;

        riff = (struct riff_chunk *) ptr_data;

        if (opt__riff_chunk_size == 0)
            a (memmove (ptr_data, script_data, script_data_len) != ptr_data);     // use 'script_data' as base
        else
            riff->chunk_size = opt__riff_chunk_size;

        riff->chunk_id = CHUNK_ID_RIFF;
        *((uint32_t *) (riff->chunk_data)) = CHUNK_ID_WEBP;

        ptr_data += SIZEOF_CHUNK_HEADER + SIZEOF_CHUNK_ID;


        uint8_t vp8x[0x0a];

        *((uint32_t *) &vp8x) = opt__vp8x_chunk_data;
        // image width (24 bits)
        vp8x[4] = (img_first->vp8l_image_width) & 0xff;
        vp8x[5] = (img_first->vp8l_image_width >> 8) & 0xff;
        vp8x[6] = (img_first->vp8l_image_width >> 16) & 0xff;
        // image height (24 bits)
        vp8x[7] = (img_first->vp8l_image_height) & 0xff;
        vp8x[8] = (img_first->vp8l_image_height >> 8) & 0xff;
        vp8x[9] = (img_first->vp8l_image_height >> 16) & 0xff;

        a ((ptr_data = insert_chunk (ptr_data, vp8x, sizeof (vp8x), CHUNK_ID_VP8X)) == NULL);


        uint32_t padding_size = riff->chunk_size - 0x1e;

        struct riff_chunk *script_chunk = (struct riff_chunk *) ptr_data;

        if (opt__riff_chunk_size == 0)
        {
            ptr_data += SIZEOF_CHUNK_HEADER + pad (script_chunk->chunk_size); // if the size is part of our data
        }
        else
        {
            if (opt__script_chunk_size == 0)
                opt__script_chunk_size = script_data_first_len + script_data_len + 20 * num_images;  // 20 => size of max uint32_t decimal width + '=' + width of decimal index num

            a ((ptr_data = insert_chunk (ptr_data, NULL, opt__script_chunk_size, opt__script_chunk_id)) == NULL);   // just prepare space for our data
        }

        padding_size -= SIZEOF_CHUNK_HEADER + pad (script_chunk->chunk_size);



        for (img = img_first; img != NULL; img = img->next)
        {
            if (img != img_first)
                img->vp8l->chunk_id += 1;
            img->offset = ptr_data - buf;
            a ((ptr_data = insert_chunk (ptr_data, img->vp8l->chunk_data, img->vp8l->chunk_size, img->vp8l->chunk_id)) == NULL);
            padding_size -= SIZEOF_CHUNK_HEADER + pad (img->vp8l->chunk_size);
        }

        a ((ptr_data = insert_chunk (ptr_data, NULL, padding_size, 0x66666666)) == NULL);



        ptr_data = script_chunk->chunk_data;

        if (opt__script_data_first)
        {
            a (memmove (ptr_data, opt__script_data_first, script_data_first_len) != ptr_data);
            ptr_data += script_data_first_len;
        }

        if (opt__script_data_img_pos)
        {
            *ptr_data = '\n';
            ptr_data++;
            for (i=0, img = img_first; img != NULL; img = img->next, i++)
                ptr_data += snprintf ((char *) ptr_data, 20, "IMG%d=%d;", i, img->offset);
            *ptr_data = '\n';
            ptr_data++;
        }

        if (opt__riff_chunk_size != 0 && script_data)
            a (memmove (ptr_data, script_data, script_data_len) != ptr_data);


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

