/*
 * oklab.cpp - K-means quantizer in Oklab perceptual color space.
 *
 * Oklab conversion math is from Björn Ottosson:
 *   https://bottosson.github.io/posts/oklab/
 *
 * sRGB transfer function: standard piecewise gamma (IEC 61966-2-1).
 */

#include "oklab.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

struct Oklab
{
	double L, a, b;
};

// sRGB byte -> linear-light float lookup.  Populated lazily once per process.
static double s_srgb_to_linear[256];
static bool   s_srgb_lut_ready = false;

static void EnsureSrgbLut()
{
	if (s_srgb_lut_ready) return;
	for (int byte = 0; byte < 256; ++byte)
	{
		double s = byte / 255.0;
		s_srgb_to_linear[byte] = (s <= 0.04045) ? (s / 12.92) : pow((s + 0.055) / 1.055, 2.4);
	}
	s_srgb_lut_ready = true;
}

static unsigned char LinearToSrgbByte(double linear)
{
	if (linear <= 0.0)            return 0;
	if (linear >= 1.0)            return 255;
	double s = (linear <= 0.0031308) ? (linear * 12.92) : (1.055 * pow(linear, 1.0/2.4) - 0.055);
	int byte = (int)(s * 255.0 + 0.5);
	if (byte < 0)   byte = 0;
	if (byte > 255) byte = 255;
	return (unsigned char)byte;
}

static Oklab LinearRgbToOklab(double r, double g, double b)
{
	double l_ = 0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b;
	double m_ = 0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b;
	double s_ = 0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b;

	double l_root = cbrt(l_);
	double m_root = cbrt(m_);
	double s_root = cbrt(s_);

	Oklab out;
	out.L = 0.2104542553 * l_root + 0.7936177850 * m_root - 0.0040720468 * s_root;
	out.a = 1.9779984951 * l_root - 2.4285922050 * m_root + 0.4505937099 * s_root;
	out.b = 0.0259040371 * l_root + 0.7827717662 * m_root - 0.8086757660 * s_root;
	return out;
}

static void OklabToLinearRgb(const Oklab& lab, double& r, double& g, double& b)
{
	double l_root = lab.L + 0.3963377774 * lab.a + 0.2158037573 * lab.b;
	double m_root = lab.L - 0.1055613458 * lab.a - 0.0638541728 * lab.b;
	double s_root = lab.L - 0.0894841775 * lab.a - 1.2914855480 * lab.b;

	double l = l_root * l_root * l_root;
	double m = m_root * m_root * m_root;
	double s = s_root * s_root * s_root;

	r = +4.0767245293 * l - 3.3072168827 * m + 0.2307590544 * s;
	g = -1.2681437731 * l + 2.6093323231 * m - 0.3411344290 * s;
	b = -0.0041119885 * l - 0.7034763098 * m + 1.7068625689 * s;
}

static Oklab SrgbByteToOklab(unsigned char r_byte, unsigned char g_byte, unsigned char b_byte)
{
	double r = s_srgb_to_linear[r_byte];
	double g = s_srgb_to_linear[g_byte];
	double b = s_srgb_to_linear[b_byte];
	return LinearRgbToOklab(r, g, b);
}

static void OklabToSrgbBytes(const Oklab& lab, unsigned char& out_r, unsigned char& out_g, unsigned char& out_b)
{
	double r, g, b;
	OklabToLinearRgb(lab, r, g, b);
	out_r = LinearToSrgbByte(r);
	out_g = LinearToSrgbByte(g);
	out_b = LinearToSrgbByte(b);
}

static double OklabDistanceSquared(const Oklab& x, const Oklab& y)
{
	double dL = x.L - y.L;
	double da = x.a - y.a;
	double db = x.b - y.b;
	return dL*dL + da*da + db*db;
}

int oklab_kmeans_quantize_rgba(const unsigned char* rgba,
                               int width, int height,
                               int max_colors,
                               const unsigned char* fixed_palette,
                               int n_fixed,
                               unsigned char* palette_rgb,
                               unsigned char* indices,
                               int* out_colors)
{
	if (rgba == NULL || width <= 0 || height <= 0 ||
	    max_colors < 1 || max_colors > 256 ||
	    palette_rgb == NULL || indices == NULL)
	{
		return 0;
	}
	if (n_fixed < 0)               n_fixed = 0;
	if (n_fixed > max_colors)      n_fixed = max_colors;
	if (n_fixed > 0 && fixed_palette == NULL) n_fixed = 0;

	EnsureSrgbLut();

	size_t pixel_count = (size_t)width * (size_t)height;

	// Convert every input pixel into Oklab once up front.
	std::vector<Oklab> pixels_lab(pixel_count);
	for (size_t pixel_idx = 0; pixel_idx < pixel_count; ++pixel_idx)
	{
		pixels_lab[pixel_idx] = SrgbByteToOklab(
			rgba[pixel_idx*4 + 0],
			rgba[pixel_idx*4 + 1],
			rgba[pixel_idx*4 + 2]);
	}

	std::vector<Oklab> centroids(max_colors);

	// Seed the locked centroids first -- these stay frozen across iterations.
	for (int slot = 0; slot < n_fixed; ++slot)
	{
		centroids[slot] = SrgbByteToOklab(
			fixed_palette[slot*3 + 0],
			fixed_palette[slot*3 + 1],
			fixed_palette[slot*3 + 2]);
	}

	// k-means++ for the remaining centroids: pick each new centroid from the
	// input with probability proportional to its squared distance from the
	// nearest centroid we've already placed.  Avoids the bad-luck duplicate
	// centroids that plain random init produces.
	std::vector<double> nearest_distance_squared(pixel_count, 0.0);
	for (int slot = n_fixed; slot < max_colors; ++slot)
	{
		double total_distance_squared = 0.0;
		if (slot == 0)
		{
			// No existing centroids yet -- pick any pixel uniformly.
			size_t random_pixel = (size_t)((double)rand() / RAND_MAX * (pixel_count - 1));
			centroids[0] = pixels_lab[random_pixel];
			for (size_t pixel_idx = 0; pixel_idx < pixel_count; ++pixel_idx)
			{
				nearest_distance_squared[pixel_idx] = OklabDistanceSquared(pixels_lab[pixel_idx], centroids[0]);
				total_distance_squared += nearest_distance_squared[pixel_idx];
			}
			continue;
		}

		// Refresh the per-pixel nearest distance against the newest centroid
		// (slot-1), and total it up so we can sample.
		for (size_t pixel_idx = 0; pixel_idx < pixel_count; ++pixel_idx)
		{
			double dist_squared = OklabDistanceSquared(pixels_lab[pixel_idx], centroids[slot - 1]);
			if (dist_squared < nearest_distance_squared[pixel_idx])
			{
				nearest_distance_squared[pixel_idx] = dist_squared;
			}
			total_distance_squared += nearest_distance_squared[pixel_idx];
		}

		if (total_distance_squared <= 0.0)
		{
			// All remaining pixels coincide with existing centroids -- fall
			// back to picking a random pixel.
			size_t random_pixel = (size_t)((double)rand() / RAND_MAX * (pixel_count - 1));
			centroids[slot] = pixels_lab[random_pixel];
			continue;
		}

		double pick_threshold = ((double)rand() / RAND_MAX) * total_distance_squared;
		double running_sum = 0.0;
		size_t picked_pixel = 0;
		for (size_t pixel_idx = 0; pixel_idx < pixel_count; ++pixel_idx)
		{
			running_sum += nearest_distance_squared[pixel_idx];
			if (running_sum >= pick_threshold)
			{
				picked_pixel = pixel_idx;
				break;
			}
		}
		centroids[slot] = pixels_lab[picked_pixel];
	}

	// Lloyd's iterations.  Reassign every pixel to its nearest centroid, then
	// recompute the non-locked centroids as the mean of their assigned pixels.
	std::vector<int>    assignments(pixel_count, 0);
	std::vector<Oklab>  sum_lab(max_colors);
	std::vector<int>    count_per_centroid(max_colors);

	const int max_iters = 32;
	for (int iter = 0; iter < max_iters; ++iter)
	{
		bool any_changed = false;

		for (size_t pixel_idx = 0; pixel_idx < pixel_count; ++pixel_idx)
		{
			double best_dist = OklabDistanceSquared(pixels_lab[pixel_idx], centroids[0]);
			int    best_slot = 0;
			for (int slot = 1; slot < max_colors; ++slot)
			{
				double dist = OklabDistanceSquared(pixels_lab[pixel_idx], centroids[slot]);
				if (dist < best_dist)
				{
					best_dist = dist;
					best_slot = slot;
				}
			}
			if (assignments[pixel_idx] != best_slot)
			{
				assignments[pixel_idx] = best_slot;
				any_changed = true;
			}
		}

		if (!any_changed && iter > 0) break;

		// Update step: skip locked centroids so they stay where the caller put them.
		for (int slot = 0; slot < max_colors; ++slot)
		{
			sum_lab[slot].L = 0.0;
			sum_lab[slot].a = 0.0;
			sum_lab[slot].b = 0.0;
			count_per_centroid[slot] = 0;
		}
		for (size_t pixel_idx = 0; pixel_idx < pixel_count; ++pixel_idx)
		{
			int slot = assignments[pixel_idx];
			sum_lab[slot].L += pixels_lab[pixel_idx].L;
			sum_lab[slot].a += pixels_lab[pixel_idx].a;
			sum_lab[slot].b += pixels_lab[pixel_idx].b;
			count_per_centroid[slot]++;
		}
		for (int slot = n_fixed; slot < max_colors; ++slot)
		{
			if (count_per_centroid[slot] > 0)
			{
				double n = (double)count_per_centroid[slot];
				centroids[slot].L = sum_lab[slot].L / n;
				centroids[slot].a = sum_lab[slot].a / n;
				centroids[slot].b = sum_lab[slot].b / n;
			}
			// If a centroid has no pixels, leave it where it was.  k-means++
			// init makes this rare, and re-seeding from a random pixel could
			// fight the locked-color frame.
		}
	}

	// Write the palette back out as 0..255 RGB bytes.
	for (int slot = 0; slot < max_colors; ++slot)
	{
		unsigned char r, g, b;
		OklabToSrgbBytes(centroids[slot], r, g, b);
		palette_rgb[slot*3 + 0] = r;
		palette_rgb[slot*3 + 1] = g;
		palette_rgb[slot*3 + 2] = b;
	}

	for (size_t pixel_idx = 0; pixel_idx < pixel_count; ++pixel_idx)
	{
		indices[pixel_idx] = (unsigned char)assignments[pixel_idx];
	}

	if (out_colors) *out_colors = max_colors;
	return 1;
}
