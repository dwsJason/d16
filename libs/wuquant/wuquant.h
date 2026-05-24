/*
 * wuquant.h - Xiaolin Wu's variance-minimizing color quantizer (v2)
 *
 * Original C source by Xiaolin Wu, Graphics Gems II, pp. 126-133.
 * Distributed freely by the author ("Free to distribute, comments and
 * suggestions are appreciated." -- Xiaolin Wu).
 *
 * This file packages the reference algorithm as a callable C library.
 */

#ifndef WUQUANT_H
#define WUQUANT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Quantize an RGBA image to <= max_colors using Wu's algorithm.
 *
 *   rgba          width*height*4 bytes, R,G,B,A in that order (A ignored).
 *   width,height  image dimensions in pixels.
 *   max_colors    desired palette size, 1..256.
 *   palette_rgb   out: max_colors*3 bytes, R,G,B per entry. Caller-allocated.
 *   indices       out: width*height bytes of palette indices. Caller-allocated.
 *   out_colors    out: number of palette entries actually produced (<=max_colors).
 *
 * Returns 1 on success, 0 on allocation failure or bad arguments.
 *
 * NOT thread-safe: the implementation uses static moment tables.
 */
int wu_quantize_rgba(const unsigned char* rgba,
                     int width, int height,
                     int max_colors,
                     unsigned char* palette_rgb,
                     unsigned char* indices,
                     int* out_colors);

#ifdef __cplusplus
}
#endif

#endif /* WUQUANT_H */
