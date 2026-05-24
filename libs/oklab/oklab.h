/*
 * oklab.h - K-means color quantizer in Oklab perceptual color space.
 *
 * Oklab is a perceptually-uniform color space by Björn Ottosson (2020).
 * Clustering in Oklab means "16 colors that look closest to the image" in
 * a way Euclidean RGB distance can't deliver -- which matters more at
 * low-res-hard-pixel resolutions where dither can't hide bad palette picks.
 *
 * https://bottosson.github.io/posts/oklab/
 */

#ifndef OKLAB_H
#define OKLAB_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Quantize an RGBA image to <= max_colors using k-means in Oklab space.
 *
 *   rgba          width*height*4 bytes, R,G,B,A (A ignored).
 *   width,height  image dimensions in pixels.
 *   max_colors    desired palette size, 1..256.
 *   fixed_palette pointer to n_fixed*3 bytes of R,G,B that must appear at
 *                 indices 0..n_fixed-1 of the output palette.  May be NULL.
 *   n_fixed       number of fixed entries (0..max_colors).
 *   palette_rgb   out: max_colors*3 bytes, R,G,B per entry. Caller-allocated.
 *   indices       out: width*height bytes of palette indices. Caller-allocated.
 *   out_colors    out: number of palette entries actually produced.
 *
 * Returns 1 on success, 0 on bad arguments or allocation failure.
 */
int oklab_kmeans_quantize_rgba(const unsigned char* rgba,
                               int width, int height,
                               int max_colors,
                               const unsigned char* fixed_palette,
                               int n_fixed,
                               unsigned char* palette_rgb,
                               unsigned char* indices,
                               int* out_colors);

#ifdef __cplusplus
}
#endif

#endif /* OKLAB_H */
