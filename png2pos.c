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
#include <limits.h>
#include <unistd.h>
#include <getopt.h>
#include "lodepng.h"

#ifdef DEBUG
#include <mcheck.h>
#endif

const char *PNG2POS_VERSION = "1.5.4";
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

// printer page width must be divisible by 8!!
const unsigned int PAPER_WIDTH = 512u;

struct {
    unsigned int cut;
    unsigned int photo;
    char align;
    unsigned int rotate;
    const char *output;
} config = {
    .cut = 0,
    .photo = 0,
    .align = 'L',
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

int main(int argc, char *argv[]) {
    {
        // printer page width must be divisible by 8!!
        if (PAPER_WIDTH % 8 != 0) {
            fprintf(stderr, "FATAL ERROR: PRINTER PAGE WIDTH MUST BE DIVISIBLE BY 8, PLEASE RECOMPILE\n");
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
                config.align = optarg[0];
                if (!strchr("lLcCrR", config.align)) {
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
                    "png2pos is a utility to convert PNG to ESC/POS binary format for EPSON TM-T70 printer\n"
                    "Usage: %s [-V] [-h] [-c] [-a L|C|R] [-r] [-p] [-o FILE] input files\n"
                    "\n"
                    "  -V          display the version number and exit\n"
                    "  -h          display this short help and exit\n"
                    "  -c          cut the paper at the end of job\n"
                    "  -a L|C|R    horizontal image alignment (Left, Center, Right)\n"
                    "  -r          rotate image upside down before it is printed\n"
                    "  -p          pre-process the images\n"
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

    // open output file and disable buffering
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

    setvbuf(fout, NULL, _IOFBF, 4096);

    // init printer
    const unsigned char POS_INIT[] = { 0x1b, 0x40 };
    for (size_t i = 0; i != sizeof(POS_INIT); ++i) {
        fputc(POS_INIT[i], fout);
    }
    fflush(fout);

    // for each input files
    while (optind != argc) {
        const char *input = argv[optind];

        // load RGBA PNG
        unsigned int img_w = 0;
        unsigned int img_h = 0;
        unsigned int lodepng_error = lodepng_decode32_file(&img_rgba, &img_w, &img_h, input);
        if (lodepng_error) {
            fprintf(stderr, "Could not load and process input PNG file, %s\n", lodepng_error_text(lodepng_error));
            goto fail;
        }

        if (img_w > PAPER_WIDTH) {
            fprintf(stderr, "Image width %u px exceeds the printer's capability (%u px)\n", img_w, PAPER_WIDTH);
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

        const unsigned int img_bw_size = img_h * (PAPER_WIDTH >> 3);
        img_bw = (unsigned char *)calloc(img_bw_size, sizeof(unsigned char));
        if (!img_bw) {
            fprintf(stderr, "Could not allocate enough memory\n");
            goto fail;
        }

        // left offset
        unsigned int offset = 0;
        switch (config.align) {
            case 'c':
            case 'C':
                offset = (PAPER_WIDTH - img_w) >> 1;
                break;

            case 'r':
            case 'R':
                offset = PAPER_WIDTH - img_w;
                break;

            case 'l':
            case 'L':
            default:
                offset = 0;
        }

        // set whole B/W image to white
        memset(img_bw, 0xff, img_bw_size);
        // compress
        for (unsigned int i = 0; i != img_grey_size; ++i) {
            const unsigned int idx = config.rotate ? (img_grey_size - 1) - i: i;
            if ((img_grey[idx] & 0x80) == 0) {
                unsigned int x = i % img_w + offset;
                unsigned int y = i / img_w;
                unsigned int j = (y * PAPER_WIDTH + x) >> 3;
                img_bw[j] &= ~(1 << (7 - (x & 0x07)));
            }
        }

        free(img_grey), img_grey = NULL;

        #ifdef DEBUG
        lodepng_encode_file("./debug_bw.png", img_bw, PAPER_WIDTH, img_h, LCT_GREY, 1);
        #endif

        // number of lines printed in one F112 command...
        // FIXME: = (65535 - size of F112) / (PAPER_WIDTH >> 3) ?
        unsigned int chunk_height = 768;
        // ...could not be over 1662 according to doc
        if (chunk_height > 1662) chunk_height = 1662;

        for (unsigned int l = 0; l < img_h; l += chunk_height) {
            unsigned int l0 = img_h - l;
            if (l0 > chunk_height) l0 = chunk_height;

            unsigned int params_size = 10 + (PAPER_WIDTH >> 3) * l0;
            unsigned char POS_F112[] = {
                0x1d, 0x28, 0x4c,
                params_size & 0xff, params_size >> 8 & 0xff,
                0x30, 0x70, 0x30, 0x01, 0x01, 0x31,
                PAPER_WIDTH & 0xff, PAPER_WIDTH >> 8 & 0xff,
                l0 & 0xff, l0 >> 8 & 0xff
            };
            for (size_t i = 0; i != sizeof(POS_F112); ++i) {
                fputc(POS_F112[i], fout);
            }

            // please note unary bitwise complement (NOT) (black 0 must be 1 = burned pixel on the paper)
            unsigned int octets = l0 * (PAPER_WIDTH >> 3);
            for (unsigned int i = 0; i != octets; ++i) {
                fputc(~img_bw[l * (PAPER_WIDTH >> 3) + i], fout);
            }

            // print data in buffer
            const unsigned char POS_F50[] = { 0x1d, 0x28, 0x4c, 0x02, 0x00, 0x30, 0x32 };
            for (size_t i = 0; i != sizeof(POS_F50); ++i) {
                fputc(POS_F50[i], fout);
            }
            fflush(fout);
        }

        free(img_bw), img_bw = NULL;

        ++optind;
   }

    if (config.cut) {
        // cut the paper
        const unsigned char POS_CUT[] = { 0x1d, 0x56, 0x41, 0x40 };
        for (size_t i = 0; i != sizeof(POS_CUT); ++i) {
            fputc(POS_CUT[i], fout);
        }
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
