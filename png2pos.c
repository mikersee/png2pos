/*!
(c) 2012 - 2015 Petr Kutalek: png2pos

Licensed under the MIT License:

Permission is hereby granted, free of charge, to any person obtaining a copy of this software
and associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

- The above copyright notice and this permission notice shall be included in all copies
or substantial portions of the Software.

- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include "lodepng.h"
#ifdef DEBUG
#include <mcheck.h>
#endif

const char *PNG2POS_VERSION = "1.6.1";
const char *PNG2POS_BUILTON = __DATE__;

// modified lodepng allocators
void* lodepng_malloc(size_t size) {
    return calloc(size, sizeof(char));
}

void* lodepng_realloc(void *ptr, size_t new_size) {
    return realloc(ptr, new_size);
}

void lodepng_free(void *ptr) {
    free(ptr);
}

// ESC sequences
#define ESC_INIT_LENGTH 2
const unsigned char ESC_INIT[ESC_INIT_LENGTH] = {
    // ESC @, Initialize printer, p. 412
    0x1b, 0x40
};

#define ESC_CUT_LENGTH 4
const unsigned char ESC_CUT[ESC_CUT_LENGTH] = {
    // GS V, Sub-Function B, p. 373
    0x1d, 0x56, 0x41,
    // Feeds paper to (cutting position + n × vertical motion unit)
    // and executes a full cut (cuts the paper completely)
    // The vertical motion unit is specified by GS P.
    0x40
};

#define ESC_OFFSET_LENGTH 4
unsigned char ESC_OFFSET[ESC_OFFSET_LENGTH] = {
    // GS L, Set left margin, p. 169
    0x1d, 0x4c, 
    // nl, nh
       0,    0
};

#define ESC_STORE_LENGTH 17
unsigned char ESC_STORE[ESC_STORE_LENGTH] = {
    // GS 8 L, Store the graphics data in the print buffer (raster format), p. 252
    0x1d, 0x38, 0x4c,
    // p1 p2 p3 p4
    0x0b,    0,    0,    0, 
    // Function 112
    0x30, 0x70, 0x30,
    // bx by, zoom
    0x01, 0x01, 
    // c, single-color printing model
    0x31, 
    // xl, xh, number of dots in the horizontal direction
       0,    0, 
    // yl, yh, number of dots in the vertical direction
       0,    0 
};

#define ESC_FLUSH_LENGTH 7
const unsigned char ESC_FLUSH[ESC_FLUSH_LENGTH] = {
    // GS ( L, Print the graphics data in the print buffer, p. 241
    // Moves print position to the left side of the print area after 
    // printing of graphics data is completed
    0x1d, 0x28, 0x4c, 0x02,    0, 0x30,
    // Fn 50
    0x32 
};

// number of dots/lines in vertical direction in one F112 command
// set to <= 128u for Epson TM-J2000/J2100
#ifndef GS8L_MAX_Y
#define GS8L_MAX_Y 256u
#endif

// max image width printer is able to process
#ifndef PRINTER_MAX_WIDTH
#define PRINTER_MAX_WIDTH 512u
#endif

// app configuration
struct {
    unsigned int cut;
    unsigned int photo;
    char align;
    unsigned int rotate;
    const char *output;
} config = {
    .cut = 0,
    .photo = 0,
    .align = '?',
    .rotate = 0,
    .output = NULL
};

FILE *fout = NULL;

inline int rebound(const int value, const int min, const int max) {
    int a = value;
    if (a < min) {
        a = min;
    }
    else if (a > max) {
        a = max;
    }
    return a;
}

char* basename(const char *s) {
    char *r = (char*)s;
    while (*s) {
        if (*s++ == '/') {
            r = (char*)s;
        }
    }
    return r;
}

void print(FILE *stream, const unsigned char *buffer, const int length) {
    for (unsigned int i = 0; i != length; ++i) {
        fputc(buffer[i], stream);
    }
}

int main(int argc, char *argv[]) {
    {
        // PRINTER_MAX_WIDTH must be divisible by 8!!
        if (PRINTER_MAX_WIDTH % 8 != 0) {
            fprintf(stderr, "FATAL ERROR: PRINTER_MAX_WIDTH MUST BE DIVISIBLE BY 8, PLEASE RECOMPILE\n");
            return EXIT_FAILURE;
        }
    }

#ifdef DEBUG
    mtrace();
#endif

    unsigned char *img_rgba = NULL;
    unsigned char *img_grey = NULL;
    unsigned char *img_bw = NULL;

    const char *BINARY_NAME = basename(argv[0]);

    int ret = EXIT_FAILURE;

    opterr = 0;
    int optc = -1;
    while ((optc = getopt(argc, argv, ":Vhca:rpo:")) != -1) {
        switch (optc) {
            case 'o':
                config.output = optarg;
                break;

            case 'c':
                config.cut = 1;
                break;

            case 'a':
                config.align = toupper(optarg[0]);
                if (!strchr("LCR", config.align)) {
                    fprintf(stderr, "Unknown horizontal alignment '%c'\n", config.align);
                    goto fail;
                }
                break;

            case 'r':
                config.rotate = 1;
                break;

            case 'p':
                config.photo = 1;
                break;

            case 'V':
                fprintf(stderr, "%s %s (%s)\n", BINARY_NAME, PNG2POS_VERSION, PNG2POS_BUILTON);
                fprintf(stderr, "%s %s\n", "LodePNG", LODEPNG_VERSION_STRING);
                ret = EXIT_SUCCESS;
                goto fail;

            case 'h':
                fprintf(stderr,
                    "png2pos is a utility to convert PNG to ESC/POS\n"
                    "Usage: %s [-V] [-h] [-c] [-a L|C|R] [-r] [-p] [-o FILE] input files\n"
                    "\n"
                    "  -V          display the version number and exit\n"
                    "  -h          display this short help and exit\n"
                    "  -c          cut the paper at the end of job\n"
                    "  -a L|C|R    horizontal image alignment (Left, Center, Right)\n"
                    "  -r          rotate image upside down before it is printed\n"
                    "  -p          switch to photo mode (pre-process input files)\n"
                    "  -o FILE     output file\n"
                    "\n"
                    "With no FILE, or when FILE is -, write to standard output\n"
                    "\n"
                    "Please read the manual page (man %s)\n"
                    "Report bugs to Petr Kutalek <petr@kutalek.cz>\n"
                    "(c) Petr Kutalek <petr@kutalek.cz>, 2012 - 2015, Licensed under the MIT license\n"
                    ,
                    BINARY_NAME, BINARY_NAME
                );
                ret = EXIT_SUCCESS;
                goto fail;

            case ':':
                fprintf(stderr, "Option '%c' requires an argument\n", optopt);
                fprintf(stderr, "For usage options run '%s -h'\n", BINARY_NAME);
                goto fail;

            default:
            case '?':
                fprintf(stderr, "'%c' is an unknown option\n", optopt);
                fprintf(stderr, "For usage options run '%s -h'\n", BINARY_NAME);
                goto fail;
        }
    }

    argc -= optind;
    argv += optind;
    optind = 0;

    // open output file and disable line buffering
    if (!config.output || strcmp(config.output, "-") == 0) {
        fout = stdout;
    } else if (!(fout = fopen(config.output, "wb"))) {
        fprintf(stderr, "Could not open output file '%s'\n", config.output);
        goto fail;
    }

    if (isatty(fileno(fout))) {
        fprintf(stderr, "This utility produces binary sequence printer commands. Output have to be redirected\n");
        goto fail;
    }

    if (setvbuf(fout, NULL, _IOFBF, 8192) != 0) {
        fprintf(stderr, "Could not set new buffer policy on output stream\n");
    }

    // init printer
    print(fout, ESC_INIT, ESC_INIT_LENGTH);
    fflush(fout);

    // for each input files
    while (optind != argc) {
        const char *input = argv[optind++];

        // load RGBA PNG
        unsigned int img_w = 0;
        unsigned int img_h = 0;
        unsigned int lodepng_error = lodepng_decode32_file(&img_rgba, &img_w, &img_h, input);
        if (lodepng_error) {
            fprintf(stderr, "Could not load and process input PNG file, %s\n", lodepng_error_text(lodepng_error));
            goto fail;
        }

        if (img_w > PRINTER_MAX_WIDTH) {
            fprintf(stderr, "Image width %u px exceeds the printer's capability (%u px)\n", img_w, PRINTER_MAX_WIDTH);
            goto fail;
        }

        unsigned int histogram[256] = { 0 };

        // convert RGBA to greyscale
        const unsigned int img_grey_size = img_h * img_w;
        img_grey = (unsigned char *)calloc(img_grey_size, sizeof(unsigned char));
        if (!img_grey) {
            fprintf(stderr, "Could not allocate enough memory\n");
            goto fail;
        }

        // imgr_rgba = [R G B A R G B A R G B A R G B A ...]
        for (unsigned int i = 0; i != img_grey_size; ++i) {
            const unsigned char *r = &img_rgba[i << 2];
            const unsigned char *g = &img_rgba[(i << 2) | 1];
            const unsigned char *b = &img_rgba[(i << 2) | 2];
            const unsigned char *a = &img_rgba[(i << 2) | 3];
            // convert RGBA to greyscale
            img_grey[i] = ((54 * *r + 182 * *g + 20 * *b) >> 8) * *a / 255 + (255 - *a);
            // prepare a histogram for HEA
            ++histogram[img_grey[i]];
        }

        free(img_rgba), img_rgba = NULL;

#ifdef DEBUG
        lodepng_encode_file("./debug_g.png", img_grey, img_w, img_h, LCT_GREY, 8);
#endif

        // post-processing
        // convert to B/W bitmap
        if (config.photo) {
            // Histogram Equalization Algorithm
            for (unsigned int i = 1; i != 256; ++i) {
                histogram[i] += histogram[i - 1];
            }
            for (unsigned int i = 0; i != img_grey_size; ++i) {
                img_grey[i] = 255 * histogram[img_grey[i]] / img_grey_size;
            }

#ifdef DEBUG
            lodepng_encode_file("./debug_g_pp.png", img_grey, img_w, img_h, LCT_GREY, 8);
#endif

            // Atkinson Dithering Algorithm
            const struct {
                int dx;
                int dy;
            } matrix[6] = {
                { .dx =  1, .dy = 0 },
                { .dx =  2, .dy = 0 },
                { .dx = -1, .dy = 1 },
                { .dx =  0, .dy = 1 },
                { .dx =  1, .dy = 1 },
                { .dx =  0, .dy = 2 }
            };
            for (unsigned int i = 0; i != img_grey_size; ++i) {
                const int o = img_grey[i];
                const int n = (o & 0x80) == 0 ? 0 : 0xff;
                img_grey[i] = n;
                const int x = i % img_w;
                const int y = i / img_w;

                for (unsigned int j = 0; j != 6; ++j) {
                    const int x0 = x + matrix[j].dx;
                    const int y0 = y + matrix[j].dy;
                    if (x0 >= img_w || x0 < 0 || y0 >= img_h) continue;
                    img_grey[x0 + img_w * y0] = rebound(img_grey[x0 + img_w * y0] + (o - n) / 8, 0, 0xff);
                }
            }
        }

        // canvas size is width of a picture rounded up to nearest multiple of 8
        const unsigned int canvas_w = ((img_w + 7) >> 3) << 3;

        const unsigned int img_bw_size = img_h * (canvas_w >> 3);
        img_bw = (unsigned char *)calloc(img_bw_size, sizeof(unsigned char));
        if (!img_bw) {
            fprintf(stderr, "Could not allocate enough memory\n");
            goto fail;
        }

        // align rotated image to the right border
        if (config.rotate == 1 && config.align == '?') {
            config.align = 'R';
        }

        // compress bytes into bitmap
        for (unsigned int i = 0; i != img_grey_size; ++i) {
            const unsigned int idx = config.rotate == 1 ? (img_grey_size - 1) - i: i;
            if ((img_grey[idx] & 0x80) == 0) {
                unsigned int x = i % img_w;
                unsigned int y = i / img_w;
                unsigned int j = (y * canvas_w + x) >> 3;
                img_bw[j] |= 0x80 >> (x & 0x07);
            }
        }

        free(img_grey), img_grey = NULL;

#ifdef DEBUG
        lodepng_encode_file("./debug_bw_inv.png", img_bw, canvas_w, img_h, LCT_GREY, 1);
#endif

        // left offset
        unsigned int offset = 0;
        switch (config.align) {
            case 'C':
                offset = (PRINTER_MAX_WIDTH - canvas_w) >> 1;
                break;

            case 'R':
                offset = PRINTER_MAX_WIDTH - canvas_w;
                break;

            case 'L':
            case '?':
            default:
                offset = 0;
        }

        // offset have to be a multiple of 8
        offset >>= 3;
        offset <<= 3;

        // chunking, l = lines already printed, currently processing a chunk of height k
        for (unsigned int l = 0, k = GS8L_MAX_Y; l < img_h; l += k) {
            if (k > img_h - l) {
                k = img_h - l;
            }

            if (offset != 0) {
                ESC_OFFSET[2] = offset & 0xff;
                ESC_OFFSET[3] = offset >> 8 & 0xff;
                print(fout, ESC_OFFSET, ESC_OFFSET_LENGTH);
            }

            const unsigned int f112_p = 10 + k * (canvas_w >> 3);
            ESC_STORE[ 3] = f112_p & 0xff;
            ESC_STORE[ 4] = f112_p >> 8 & 0xff;
            ESC_STORE[13] = canvas_w & 0xff;
            ESC_STORE[14] = canvas_w >> 8 & 0xff;
            ESC_STORE[15] = k & 0xff;
            ESC_STORE[16] = k >> 8 & 0xff;

            print(fout, ESC_STORE, ESC_STORE_LENGTH);
            print(fout, &img_bw[l * (canvas_w >> 3)], k * (canvas_w >> 3));
            print(fout, ESC_FLUSH, ESC_FLUSH_LENGTH);
            fflush(fout);
        }

        free(img_bw), img_bw = NULL;
   }

    if (config.cut == 1) {
        // cut the paper
        print(fout, ESC_CUT, ESC_CUT_LENGTH);
        fflush(fout);
    }

    ret = EXIT_SUCCESS;

fail:
    free(img_rgba), img_rgba = NULL;
    free(img_grey), img_grey = NULL;
    free(img_bw), img_bw = NULL;

    if (fout != NULL && fout != stdout) {
       fclose(fout), fout = NULL;
    }

#ifdef DEBUG
    muntrace();
#endif

    return ret;
}
