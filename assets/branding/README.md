# Repository wordmark

`sonic-adventure-recompiled-logo.png` is the transparent header wordmark for
the GitHub README. Display it centered at 512 CSS pixels; GitHub scales it
down on smaller screens. Keep the aspect ratio and transparent safe margin.

The image is a cutout variant of the existing desktop wordmark in
`../icons/sonic-adventure-recompiled.png`, prepared with built-in Imagegen.
The desktop icon and game/setup artwork are unchanged. The final PNG has a
real alpha channel; the generated preview's checkerboard was removed during
alpha export, without making the white lettering transparent.

Final image-edit prompt:

> Remove the checkerboard background from this logo completely. Return the same
> logo as a PNG cutout with a real transparent alpha channel. Preserve all
> lettering and the complete orange outline. The gray checkerboard is unwanted
> image content, not transparency. Export only the freestanding SONIC ADVENTURE
> RECOMPILED logo, tightly framed, with actual transparent pixels outside it.

Verify both light and dark backgrounds when replacing this asset. Do not use
an opaque checkerboard image as a substitute for transparency.
