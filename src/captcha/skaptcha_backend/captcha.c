/*
captcha

Copyright (c) Kopikova Sofia, 2020

   -- Slightly modified by Andrey V. Stolyarov to remove some
   -- unneeded dependencies and the like.
   -- Portions copyright (c) Andrey V. Stolyarov, 2023

   -- Segment shuffle logic replaced by Y. Kotov
   -- Portions copyright (c) Y. Kotov, 2024

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software
    in a product, an acknowledgment in the product documentation would be
    appreciated but is not required.

    2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.

    3. This notice may not be removed or altered from any source
    distribution.
*/



#if 0
#include <stdio.h>
#endif
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include "lodepng.h"
#include "pictures.cpp"

#include "captcha.h"

#define NUM 33
#define LENGTH CAPTCHA_STRING_LENGTH
#define SEGS NUM_OF_SEGMENTS
#define LARR 6
#define DEG M_PI/6
#define Y1 22
#define Y2 72
#define WIDTH_ONE 22
#define HEIGHT_ONE 20
#define HEIGHT 90
#ifndef M_PI
#define M_PI 3.14159
#endif

typedef struct
{
	int x1, x2;
	int sz;

} xpair;

static const unsigned char* const letters[] = {
		letter_image_A,
		letter_image_B,
		letter_image_C,
		letter_image_D,
		letter_image_E,
		letter_image_F,
		letter_image_G,
		letter_image_H,
		letter_image_I,
		letter_image_J,
		letter_image_K,
		letter_image_L,
		letter_image_M,
		letter_image_N,
		letter_image_P,
		letter_image_Q,
		letter_image_R,
		letter_image_S,
		letter_image_T,
		letter_image_U,
		letter_image_V,
		letter_image_W,
		letter_image_X,
		letter_image_Y,
		letter_image_Z,
		letter_image_2,
		letter_image_3,
		letter_image_4,
		letter_image_5,
		letter_image_6,
		letter_image_7,
		letter_image_8,
		letter_image_9
};
static const int letter_sizes[] = {
		sizeof(letter_image_A),
		sizeof(letter_image_B),
		sizeof(letter_image_C),
		sizeof(letter_image_D),
		sizeof(letter_image_E),
		sizeof(letter_image_F),
		sizeof(letter_image_G),
		sizeof(letter_image_H),
		sizeof(letter_image_I),
		sizeof(letter_image_J),
		sizeof(letter_image_K),
		sizeof(letter_image_L),
		sizeof(letter_image_M),
		sizeof(letter_image_N),
		sizeof(letter_image_P),
		sizeof(letter_image_Q),
		sizeof(letter_image_R),
		sizeof(letter_image_S),
		sizeof(letter_image_T),
		sizeof(letter_image_U),
		sizeof(letter_image_V),
		sizeof(letter_image_W),
		sizeof(letter_image_X),
		sizeof(letter_image_Y),
		sizeof(letter_image_Z),
		sizeof(letter_image_2),
		sizeof(letter_image_3),
		sizeof(letter_image_4),
		sizeof(letter_image_5),
		sizeof(letter_image_6),
		sizeof(letter_image_7),
		sizeof(letter_image_8),
		sizeof(letter_image_9)
};

const char used_letters[] = "ABCDEFGHIJKLMNPQRSTUVWXYZ23456789";

#define NUM_OF_USED_LETTERS (sizeof(used_letters)-1)

static unsigned* generate()
{
/*	printf("NUM_OF_USED_LETTERS: %d\n", (int)NUM_OF_USED_LETTERS);*/
	unsigned x = 0;
	/*srand(time(NULL));  too easy to guess! */
	unsigned* s = (unsigned*)calloc(LENGTH + 1, sizeof(int));
	int i;
	for(i = 0; i < LENGTH; i++)
	{
		x = rand() % NUM_OF_USED_LETTERS;
		s[i] = x;
	}
	s[LENGTH] = '\0';
	return s;
}

static int asum(int arr[], int num)
{
	int sum = 0;
	int i;
	for(i = 0; i < num; i++)
	{
		sum += arr[i];
	}
	return sum;
}

static void mix(char *ans, unsigned* s, xpair coords[])
{
	int len_segs[SEGS];

	int i;
	for(i = 0; i < SEGS - 1; i++)
	{
		len_segs[i] = rand() % (LENGTH - (SEGS - i - 1)
							- asum(len_segs, i))
					  + 1;
		coords[i].sz = len_segs[i];
	}
	len_segs[SEGS - 1] = LENGTH - asum(len_segs, SEGS - 1);
	coords[SEGS - 1].sz = len_segs[SEGS - 1];

	/* Calculate max order: factorial(num_segs). */
	unsigned max_order = 1;
	for(i = 2; i < SEGS+1; i++)
		max_order *= i;

	/* Calculate order of segments excluding the trivial one. */
	unsigned order = 1 + rand() % (max_order - 1);

	/* Prepare the list of no processed segments.
	 * The formula is a bit weird just to make order=0 match 0,1,2,3. */
	unsigned segs[SEGS];
	for (i = 0; i < SEGS; i++)
		segs[i] = (SEGS - i) % SEGS;

	int cur = 0;
	int curc = 0;
	int seg = 0;
	for(seg = 0; seg < SEGS; seg++)
	{
		unsigned idx = order % (SEGS - seg);
		unsigned n = segs[idx];
		segs[idx] = segs[SEGS - seg - 1];
		order /= SEGS - seg;

		for(i = 0; i < len_segs[n]; i++)
		{
			int index = s[i + asum(len_segs, n)];
			ans[cur] = used_letters[index];
			cur++;
		}
		coords[n].x1 = WIDTH_ONE * asum(len_segs, n)
				   + WIDTH_ONE * len_segs[n] / 2;
		coords[n].x2 = curc + WIDTH_ONE * len_segs[n] / 2;
		curc += WIDTH_ONE * len_segs[n];
	}
/*	printf("\n");
	for (int i = 0; i < SEGS; i++)
	{
		printf("%d line at coords: %d %d, size: %d\n", i + 1,
				coords[i].x1, coords[i].x2, coords[i].sz);
	}*/
	ans[LENGTH] = '\0';
	/*printf("%s\n", ss);*/
}

static void hypotenuse(unsigned char** in, unsigned x1, unsigned y1,
		unsigned x2, unsigned y2, unsigned w, unsigned h)
{

    int color = 255;

	unsigned dx = abs((int)x2 - (int)x1);
	unsigned dy = y2 - y1;
	double err = 0;
	double derr = (double)dx / (double)dy;
	double errmax = 0.5;

	unsigned x = x1;
	unsigned dirx = (x2 > x1) ? 1 : -1;

	unsigned y;
	for(y = y1; y < y2; y++)
	{
		size_t byte_index = y * w + x;


		(*in)[byte_index] = (unsigned char)(color);

		err += derr;
		while(err >= errmax)
		{
			byte_index = y * w + x;
			(*in)[byte_index] = (unsigned char)(color);
			x += dirx;
			err -= 1.0;
		}
	}
}

static void
arrows(unsigned char** in, unsigned x1, unsigned y1, unsigned x2, unsigned y2,
		unsigned w, unsigned h, unsigned l)
{
	int x = x1 - x2;
	int y = y2 - y1;
	double v = sqrt(pow(x, 2) + pow(y, 2));

	double dg1 = asin(x / v) + DEG;
	double dg2 = dg1 - 2 * DEG;

	unsigned x11 = x2 + (int)(l * sin(dg1) + 0.5);
	unsigned y11 = y2 - (int)(l * cos(dg1) + 0.5);
	unsigned x22 = x2 + (int)(l * sin(dg2) + 0.5);
	unsigned y22 = y2 - (int)(l * cos(dg2) + 0.5);

	/*printf("drawing arrows to: %d %d %d %d\n", x11, y11, x2, y2);
	printf("drawing arrows to: %d %d %d %d\n", x22, y22, x2, y2);*/
	hypotenuse(in, x11, y11, x2, y2, w, h);
	hypotenuse(in, x22, y22, x2, y2, w, h);
}

static void line(unsigned char** in, unsigned x1, unsigned x2, unsigned y,
		unsigned w, unsigned h)
{
    int color = 255;
	unsigned x;
	for(x = x1; x < x2; x++)
	{
		size_t byte_index = y * w + x;
		(*in)[byte_index] = (unsigned char)(color);
	}
}

static void rect(unsigned char** in, unsigned x1, unsigned x2,
		unsigned w, unsigned h)
{
	int color = (int)(125);
	unsigned x, y;
	for(y = h - 14; y < h - 2; y++)
		for(x = x1; x < x2; x++)
		{
			size_t byte_index = y * w + x;
			(*in)[byte_index] = (unsigned char)(color);
		}
}

static unsigned char* reunis(const unsigned* str, unsigned* w, unsigned* h)
{
	unsigned err;

	unsigned char** images2 =
			(unsigned char**)malloc(LENGTH * sizeof(size_t));

	unsigned width1 = 0, height1 = 0;

	unsigned i;
	for(i = 0; i < LENGTH; i++)
	{
		size_t nn = str[i];

		LodePNGState state;
		lodepng_state_init(&state);
		state.info_raw.colortype = LCT_GREY;
		state.info_raw.bitdepth = 8;

		err = lodepng_decode(images2 + i, &width1, &height1,
				&state, letters[nn], letter_sizes[nn]);

		lodepng_state_cleanup(&state);

#if 0  /* error handling removed by avst */
		if(err)
		{
			printf("decoder error %u : %s\n", err,
						lodepng_error_text(err));
			exit(1);
		}
#endif
		if(err)
			return NULL;
	}

	*w = width1 * LENGTH;
	*h = HEIGHT;

	unsigned char* image = (unsigned char*)malloc((*w) * (*h) * LENGTH + 7);

	unsigned y;
	for(y = 0; y < height1; y++)
	{
		for(i = 0; i < LENGTH; i++)
		{
			memcpy(image + y * (*w) + i * width1, *(images2 + i)
						 + (y * width1), width1);
		}
	}

	for(i = 0; i < LENGTH; i++)
	{
		free(images2[i]);
	}
	free(images2);

	return image;
}

void generate_captcha(char** img, int* imgsize, char* answer)
{
	size_t outsize;
#if 0
	unsigned error;
#endif
	unsigned char** out = (unsigned char**)img;
	unsigned* s1 = generate();

/*	for (int i = 0; i < LENGTH; i++)
	{
		printf("%c", used_letters[s1[i]]);
	}
	printf("\n");*/

	xpair coords[SEGS];
	mix(answer, s1, coords);

	unsigned char* fin = 0;
	unsigned width = 0, height = 0;
	fin = reunis(s1, &width, &height);
	free(s1);
	/*printf("Width: %d\nHeight: %d\n", width, height);*/

    unsigned char color = 0;
	unsigned x, y;
	for(y = HEIGHT_ONE; y < height; y++)
	{
		for(x = 0; x < width; x++)
		{
			size_t byte_index = y * width + x;
			fin[byte_index] = color;
		}
	}

	int i;
	for(i = 0; i < SEGS; i++)
	{
		int x1 = coords[i].x1;
		int x2 = coords[i].x2;
		int sz = coords[i].sz;
        hypotenuse(&fin, x1, Y1, x2, Y2, width, height);
        arrows(&fin, x1, Y1, x2, Y2, width, height, LARR);
        line(&fin, x1 - sz * WIDTH_ONE / 2 + 5,
                x1 + sz * WIDTH_ONE / 2 - 5, HEIGHT_ONE + 1,
                                width, height);
		rect(&fin, x2 - sz * WIDTH_ONE / 2 + 2,
				x2 + sz * WIDTH_ONE / 2 - 2, width, height);
	}

	LodePNGState state;
	lodepng_state_init(&state);
	state.info_png.color.colortype = LCT_GREY;
	state.info_png.color.bitdepth = 8;
	state.info_raw.colortype = LCT_GREY;
	state.info_raw.bitdepth = 8;
	state.encoder.auto_convert = 0;

#if 0  /* error handling removed by avst */
	error =
#endif
	lodepng_encode(out, &outsize, fin, width, height, &state);
	*imgsize = outsize;
	free(fin);

#if 0  /* error handling removed by avst */
	if(error)
	{
		printf("encoder error %u : %s\n", error,
						lodepng_error_text(error));
		exit(2);
	}
#endif
	lodepng_state_cleanup(&state);
}
