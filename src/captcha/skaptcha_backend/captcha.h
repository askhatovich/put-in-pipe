/*
captcha

Copyright (c) Kopikova Sofia, 2020

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


#ifndef CAPTCHA_INCLUDED
#define CAPTCHA_INCLUDED

enum {
    CAPTCHA_STRING_LENGTH = 5,
    NUM_OF_SEGMENTS = 4
};

#ifdef __cplusplus
extern "C" {
#endif

/*
   *img will contain the captcha PNG as bytes (it is quite small
        -- several KB); it is malloc'ed and must be free'd by
        the caller once the image is no longer needed;
   *imgsize will be filled with count of bytes in img;
   answer must point to a char buffer of at least CAPTCHA_STRING_LENGTH+1
        cells, it will be filled with the captcha answer
*/
void generate_captcha(char **img, int *imgsize, char *answer);

#ifdef __cplusplus
}
#endif

#endif
