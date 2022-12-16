This file documents the significant user-visible changes in each release of the `loder` R package.

## loder 0.2.1

- The LodePNG library has been updated to version 20221108.
- The package no longer calls the `sprintf` C function, which produces a warning on some platforms.
- Other small warnings and notes have been addressed.

## loder 0.2.0

- The new `inspectPng` function can be used to obtain details of a PNG file without obtaining the pixel data.
- There is now a `print` method for objects of class "loder".
- The `writePng` function now supports writing interlaced files, and offers different compression levels.
- Text chunks are now read from and written to PNG files.
- The LodePNG library has been updated.

## loder 0.1.2

- The package should now compile against versions of R prior to 3.3.0.
- Some low-level memory protection and undefined behaviour warnings have been resolved.

## loder 0.1.1

- File arguments to `readPng` and `writePng` are now passed through `path.expand`, so that paths containing a tilde (`~`) will be handled properly.

## loder 0.1.0

- First public release.
