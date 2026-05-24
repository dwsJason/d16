/*
 * wuquant.c - Xiaolin Wu's variance-minimizing color quantizer (v2)
 *
 * Original algorithm and source code by Xiaolin Wu, Graphics Gems II,
 * pp. 126-133, 1991. Updated version posted by the author to public
 * Internet archives ("Free to distribute, comments and suggestions are
 * appreciated.").
 *
 * This file is a faithful repackaging of Wu's reference implementation
 * as a callable C library. The algorithm is unchanged. Differences:
 *   - K&R-style function declarations converted to ANSI/C99.
 *   - main() replaced by wu_quantize_rgba() API entry point.
 *   - Moment tables and pixel buffers are file-statics, reset on each call.
 *   - Helpers are declared static so they don't clash with other libs.
 *
 * NOT thread-safe.
 */

#include "wuquant.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXCOLOR    256
#define RED     2
#define GREEN   1
#define BLUE    0

struct box {
    int r0;          /* min value, exclusive */
    int r1;          /* max value, inclusive */
    int g0;
    int g1;
    int b0;
    int b1;
    int vol;
};

/* Histogram is in elements 1..32 along each axis,
 * element 0 is for base or marginal value.
 * NB: these must be zeroed before each call.
 */
static float       m2[33][33][33];
static long int    wt[33][33][33], mr[33][33][33], mg[33][33][33], mb[33][33][33];
static unsigned char *Ir, *Ig, *Ib;
static int         size;  /* image size (pixel count) */
static int         K;     /* color look-up table size */
static unsigned short int *Qadd;

static void Hist3d(long int *vwt, long int *vmr, long int *vmg, long int *vmb, float *m2_)
{
    int ind, r, g, b;
    int inr, ing, inb, table[256];
    long int i;

    for (i = 0; i < 256; ++i) table[i] = (int)(i * i);
    Qadd = (unsigned short int *)malloc(sizeof(short int) * size);
    if (Qadd == NULL) { fprintf(stderr, "wuquant: out of memory\n"); return; }
    for (i = 0; i < size; ++i) {
        r = Ir[i]; g = Ig[i]; b = Ib[i];
        inr = (r >> 3) + 1;
        ing = (g >> 3) + 1;
        inb = (b >> 3) + 1;
        Qadd[i] = (unsigned short int)(ind = (inr << 10) + (inr << 6) + inr + (ing << 5) + ing + inb);
        /*[inr][ing][inb]*/
        ++vwt[ind];
        vmr[ind] += r;
        vmg[ind] += g;
        vmb[ind] += b;
        m2_[ind] += (float)(table[r] + table[g] + table[b]);
    }
}

/* Convert histogram into cumulative moments so that the sum over any box
 * can be computed by inclusion-exclusion in 8 lookups.
 */
static void M3d(long int *vwt, long int *vmr, long int *vmg, long int *vmb, float *m2_)
{
    unsigned short int ind1, ind2;
    unsigned char i, r, g, b;
    long int line, line_r, line_g, line_b,
             area[33], area_r[33], area_g[33], area_b[33];
    float    line2, area2[33];

    for (r = 1; r <= 32; ++r) {
        for (i = 0; i <= 32; ++i)
            area2[i] = area[i] = area_r[i] = area_g[i] = area_b[i] = 0;
        for (g = 1; g <= 32; ++g) {
            line2 = 0; line = line_r = line_g = line_b = 0;
            for (b = 1; b <= 32; ++b) {
                ind1 = (unsigned short)((r << 10) + (r << 6) + r + (g << 5) + g + b);
                line   += vwt[ind1];
                line_r += vmr[ind1];
                line_g += vmg[ind1];
                line_b += vmb[ind1];
                line2  += m2_[ind1];
                area[b]   += line;
                area_r[b] += line_r;
                area_g[b] += line_g;
                area_b[b] += line_b;
                area2[b]  += line2;
                ind2 = (unsigned short)(ind1 - 1089); /* [r-1][g][b] */
                vwt[ind1] = vwt[ind2] + area[b];
                vmr[ind1] = vmr[ind2] + area_r[b];
                vmg[ind1] = vmg[ind2] + area_g[b];
                vmb[ind1] = vmb[ind2] + area_b[b];
                m2_[ind1]  = m2_[ind2]  + area2[b];
            }
        }
    }
}

static long int Vol(struct box *cube, long int mmt[33][33][33])
{
    return ( mmt[cube->r1][cube->g1][cube->b1]
            -mmt[cube->r1][cube->g1][cube->b0]
            -mmt[cube->r1][cube->g0][cube->b1]
            +mmt[cube->r1][cube->g0][cube->b0]
            -mmt[cube->r0][cube->g1][cube->b1]
            +mmt[cube->r0][cube->g1][cube->b0]
            +mmt[cube->r0][cube->g0][cube->b1]
            -mmt[cube->r0][cube->g0][cube->b0] );
}

static long int Bottom(struct box *cube, unsigned char dir, long int mmt[33][33][33])
{
    switch (dir) {
        case RED:
            return ( -mmt[cube->r0][cube->g1][cube->b1]
                    +mmt[cube->r0][cube->g1][cube->b0]
                    +mmt[cube->r0][cube->g0][cube->b1]
                    -mmt[cube->r0][cube->g0][cube->b0] );
        case GREEN:
            return ( -mmt[cube->r1][cube->g0][cube->b1]
                    +mmt[cube->r1][cube->g0][cube->b0]
                    +mmt[cube->r0][cube->g0][cube->b1]
                    -mmt[cube->r0][cube->g0][cube->b0] );
        case BLUE:
            return ( -mmt[cube->r1][cube->g1][cube->b0]
                    +mmt[cube->r1][cube->g0][cube->b0]
                    +mmt[cube->r0][cube->g1][cube->b0]
                    -mmt[cube->r0][cube->g0][cube->b0] );
    }
    return 0;
}

static long int Top(struct box *cube, unsigned char dir, int pos, long int mmt[33][33][33])
{
    switch (dir) {
        case RED:
            return ( mmt[pos][cube->g1][cube->b1]
                    -mmt[pos][cube->g1][cube->b0]
                    -mmt[pos][cube->g0][cube->b1]
                    +mmt[pos][cube->g0][cube->b0] );
        case GREEN:
            return ( mmt[cube->r1][pos][cube->b1]
                    -mmt[cube->r1][pos][cube->b0]
                    -mmt[cube->r0][pos][cube->b1]
                    +mmt[cube->r0][pos][cube->b0] );
        case BLUE:
            return ( mmt[cube->r1][cube->g1][pos]
                    -mmt[cube->r1][cube->g0][pos]
                    -mmt[cube->r0][cube->g1][pos]
                    +mmt[cube->r0][cube->g0][pos] );
    }
    return 0;
}

static float Var(struct box *cube)
{
    float dr, dg, db, xx;

    dr = (float)Vol(cube, mr);
    dg = (float)Vol(cube, mg);
    db = (float)Vol(cube, mb);
    xx =  m2[cube->r1][cube->g1][cube->b1]
         -m2[cube->r1][cube->g1][cube->b0]
         -m2[cube->r1][cube->g0][cube->b1]
         +m2[cube->r1][cube->g0][cube->b0]
         -m2[cube->r0][cube->g1][cube->b1]
         +m2[cube->r0][cube->g1][cube->b0]
         +m2[cube->r0][cube->g0][cube->b1]
         -m2[cube->r0][cube->g0][cube->b0];

    return xx - (dr*dr + dg*dg + db*db) / (float)Vol(cube, wt);
}

static float Maximize(struct box *cube, unsigned char dir,
                      int first, int last, int *cut,
                      long int whole_r, long int whole_g, long int whole_b, long int whole_w)
{
    long int half_r, half_g, half_b, half_w;
    long int base_r, base_g, base_b, base_w;
    int i;
    float temp, max;

    base_r = Bottom(cube, dir, mr);
    base_g = Bottom(cube, dir, mg);
    base_b = Bottom(cube, dir, mb);
    base_w = Bottom(cube, dir, wt);
    max = 0.0f;
    *cut = -1;
    for (i = first; i < last; ++i) {
        half_r = base_r + Top(cube, dir, i, mr);
        half_g = base_g + Top(cube, dir, i, mg);
        half_b = base_b + Top(cube, dir, i, mb);
        half_w = base_w + Top(cube, dir, i, wt);
        if (half_w == 0) continue; /* never split into an empty box */
        temp = ((float)half_r*half_r + (float)half_g*half_g +
                (float)half_b*half_b) / (float)half_w;

        half_r = whole_r - half_r;
        half_g = whole_g - half_g;
        half_b = whole_b - half_b;
        half_w = whole_w - half_w;
        if (half_w == 0) continue; /* never split into an empty box */
        temp += ((float)half_r*half_r + (float)half_g*half_g +
                 (float)half_b*half_b) / (float)half_w;

        if (temp > max) { max = temp; *cut = i; }
    }
    return max;
}

static int Cut(struct box *set1, struct box *set2)
{
    unsigned char dir;
    int cutr, cutg, cutb;
    float maxr, maxg, maxb;
    long int whole_r, whole_g, whole_b, whole_w;

    whole_r = Vol(set1, mr);
    whole_g = Vol(set1, mg);
    whole_b = Vol(set1, mb);
    whole_w = Vol(set1, wt);

    maxr = Maximize(set1, RED,   set1->r0+1, set1->r1, &cutr,
                    whole_r, whole_g, whole_b, whole_w);
    maxg = Maximize(set1, GREEN, set1->g0+1, set1->g1, &cutg,
                    whole_r, whole_g, whole_b, whole_w);
    maxb = Maximize(set1, BLUE,  set1->b0+1, set1->b1, &cutb,
                    whole_r, whole_g, whole_b, whole_w);

    if      ((maxr >= maxg) && (maxr >= maxb)) {
        dir = RED;
        if (cutr < 0) return 0; /* can't split the box */
    }
    else if ((maxg >= maxr) && (maxg >= maxb)) dir = GREEN;
    else                                       dir = BLUE;

    set2->r1 = set1->r1;
    set2->g1 = set1->g1;
    set2->b1 = set1->b1;

    switch (dir) {
        case RED:
            set2->r0 = set1->r1 = cutr;
            set2->g0 = set1->g0;
            set2->b0 = set1->b0;
            break;
        case GREEN:
            set2->g0 = set1->g1 = cutg;
            set2->r0 = set1->r0;
            set2->b0 = set1->b0;
            break;
        case BLUE:
            set2->b0 = set1->b1 = cutb;
            set2->r0 = set1->r0;
            set2->g0 = set1->g0;
            break;
    }
    set1->vol = (set1->r1 - set1->r0) * (set1->g1 - set1->g0) * (set1->b1 - set1->b0);
    set2->vol = (set2->r1 - set2->r0) * (set2->g1 - set2->g0) * (set2->b1 - set2->b0);
    return 1;
}

static void Mark(struct box *cube, int label, unsigned char *tag)
{
    int r, g, b;

    for (r = cube->r0 + 1; r <= cube->r1; ++r)
        for (g = cube->g0 + 1; g <= cube->g1; ++g)
            for (b = cube->b0 + 1; b <= cube->b1; ++b)
                tag[(r<<10) + (r<<6) + r + (g<<5) + g + b] = (unsigned char)label;
}

int wu_quantize_rgba(const unsigned char* rgba,
                     int width, int height,
                     int max_colors,
                     unsigned char* palette_rgb,
                     unsigned char* indices,
                     int* out_colors)
{
    struct box cube[MAXCOLOR];
    unsigned char *tag = NULL;
    unsigned char lut_r[MAXCOLOR], lut_g[MAXCOLOR], lut_b[MAXCOLOR];
    int next;
    long int i, weight;
    int k;
    float vv[MAXCOLOR], temp;

    if (rgba == NULL || width <= 0 || height <= 0 ||
        max_colors < 1 || max_colors > MAXCOLOR ||
        palette_rgb == NULL || indices == NULL) {
        return 0;
    }

    size = width * height;
    K = max_colors;

    /* zero the moment tables */
    memset(m2, 0, sizeof(m2));
    memset(wt, 0, sizeof(wt));
    memset(mr, 0, sizeof(mr));
    memset(mg, 0, sizeof(mg));
    memset(mb, 0, sizeof(mb));

    Ir = (unsigned char *)malloc((size_t)size);
    Ig = (unsigned char *)malloc((size_t)size);
    Ib = (unsigned char *)malloc((size_t)size);
    if (Ir == NULL || Ig == NULL || Ib == NULL) {
        free(Ir); free(Ig); free(Ib);
        Ir = Ig = Ib = NULL;
        return 0;
    }
    for (i = 0; i < size; ++i) {
        Ir[i] = rgba[i*4 + 0];
        Ig[i] = rgba[i*4 + 1];
        Ib[i] = rgba[i*4 + 2];
    }

    Qadd = NULL;
    Hist3d(&wt[0][0][0], &mr[0][0][0], &mg[0][0][0], &mb[0][0][0], &m2[0][0][0]);
    free(Ir); free(Ig); free(Ib);
    Ir = Ig = Ib = NULL;

    if (Qadd == NULL) {
        return 0;
    }

    M3d(&wt[0][0][0], &mr[0][0][0], &mg[0][0][0], &mb[0][0][0], &m2[0][0][0]);

    cube[0].r0 = cube[0].g0 = cube[0].b0 = 0;
    cube[0].r1 = cube[0].g1 = cube[0].b1 = 32;
    next = 0;
    for (i = 1; i < K; ++i) {
        if (Cut(&cube[next], &cube[i])) {
            vv[next] = (cube[next].vol > 1) ? Var(&cube[next]) : 0.0f;
            vv[i]    = (cube[i].vol    > 1) ? Var(&cube[i])    : 0.0f;
        } else {
            vv[next] = 0.0f; /* don't try to split this box again */
            i--;             /* didn't create box i */
        }
        next = 0; temp = vv[0];
        for (k = 1; k <= (int)i; ++k)
            if (vv[k] > temp) { temp = vv[k]; next = k; }
        if (temp <= 0.0f) {
            K = (int)i + 1;
            break;
        }
    }

    tag = (unsigned char *)malloc(33u * 33u * 33u);
    if (tag == NULL) {
        free(Qadd); Qadd = NULL;
        return 0;
    }
    for (k = 0; k < K; ++k) {
        Mark(&cube[k], k, tag);
        weight = Vol(&cube[k], wt);
        if (weight) {
            lut_r[k] = (unsigned char)(Vol(&cube[k], mr) / weight);
            lut_g[k] = (unsigned char)(Vol(&cube[k], mg) / weight);
            lut_b[k] = (unsigned char)(Vol(&cube[k], mb) / weight);
        } else {
            lut_r[k] = lut_g[k] = lut_b[k] = 0;
        }
    }

    for (i = 0; i < size; ++i)
        indices[i] = tag[Qadd[i]];

    for (k = 0; k < K; ++k) {
        palette_rgb[k*3 + 0] = lut_r[k];
        palette_rgb[k*3 + 1] = lut_g[k];
        palette_rgb[k*3 + 2] = lut_b[k];
    }

    if (out_colors) *out_colors = K;

    free(tag);
    free(Qadd); Qadd = NULL;
    return 1;
}
