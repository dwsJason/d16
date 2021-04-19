# d16 - Dream16
IMGUI experiments - Experimental Docking branch of IMGUI

Dream16 is good for scaling, cropping, and converting true color images down
into a limited number of colors.  There is no undo.

This UI is weird, so I hope your at least read about the PARADIGM before
getting started.

PARADIGM - Images, when loaded into the program are converted into true color.
The colors internally are represented by RGB triples.   All CROP and scaling
operations, work on the true color representation of the image.  Scaling, if
it does anything other than point sampling, may generate new colors to smooth
things out.

In order to get an image out of the program, you must quantize it.  For IIgs,
I recommend 444 (takes the colors down to 12-bit), 50% dither, and 16 colors.

For FMX I recommend 888, 0% dither, and 256 colors.

Once you press the button to quatize, a second version of the image appears to
the right, quatized down for preview.  Right click on the second image, and you
can choose to save as, multiple formats.

Generally, any image the Dream16 can save, Dream16 can also load for
verification purposes.




