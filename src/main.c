#include <R.h>
#include <Rdefines.h>
#include <R_ext/Rdynload.h>

#include "lodepng.h"

SEXP read_png (SEXP file_)
{
    unsigned error;
    unsigned width, height, channels;
    unsigned char *png = NULL;
    size_t png_size;
    unsigned char *image;
    LodePNGState state;
    
    lodepng_state_init(&state);
    
    const char *filename = CHAR(STRING_ELT(file_, 0));
    error = lodepng_load_file(&png, &png_size, filename);
    if (error)
    {
        free(png);
        REprintf("LodePNG error: %s\n", error, lodepng_error_text(error));
    }
    
    error = lodepng_inspect(&width, &height, &state, png, png_size);
    if (error)
    {
        free(png);
        REprintf("LodePNG error: %s\n", error, lodepng_error_text(error));
    }
    
    switch (state.info_png.color.colortype)
    {
        case LCT_GREY:
        channels = 1;
        break;
        
        case LCT_RGB:
        channels = 3;
        break;
        
        case LCT_PALETTE:
        channels = 4;
        break;
        
        case LCT_GREY_ALPHA:
        channels = 2;
        break;
        
        case LCT_RGBA:
        channels = 4;
        break;
    }
    
    R_len_t length;
    SEXP result, dim;
    length = (R_len_t) width * height * channels;
    PROTECT(result = NEW_INTEGER(length));
    
    LodePNGColorType final_color_type = (state.info_png.color.colortype == LCT_PALETTE ? LCT_RGBA : state.info_png.color.colortype);
    error = lodepng_decode_memory(&image, &width, &height, png, png_size, final_color_type, 8);
    free(png);
    if (error)
    {
        free(image);
        REprintf("LodePNG error: %s\n", error, lodepng_error_text(error));
    }
    
    // LodePNG returns pixel data with dimensions reversed relative to R, so we need to correct it back
    int *result_ptr = INTEGER(result);
    size_t result_strides[2] = { (size_t) height, (size_t) height * width };
    size_t image_strides[2] = { (size_t) channels, (size_t) channels * width };
    for (unsigned i=0; i<height; i++)
    {
        for (unsigned j=0; j<width; j++)
        {
            for (unsigned k=0; k<channels; k++)
                result_ptr[i+j*result_strides[0]+k*result_strides[1]] = (int) image[k+j*image_strides[0]+i*image_strides[1]];
        }
    }
    
    lodepng_state_cleanup(&state);
    free(image);
    
    PROTECT(dim = NEW_INTEGER(3));
    int *dim_ptr = INTEGER(dim);
    dim_ptr[0] = height;
    dim_ptr[1] = width;
    dim_ptr[2] = channels;
    setAttrib(result, R_DimSymbol, dim);
    
    UNPROTECT(2);
    return result;
}

SEXP write_png (SEXP image_, SEXP file_)
{
    return R_NilValue;
}

static R_CallMethodDef callMethods[] = {
    { "read_png",   (DL_FUNC) &read_png,    1 },
    { "write_png",  (DL_FUNC) &write_png,   2 },
    { NULL, NULL, 0 }
};

void R_init_loder (DllInfo *info)
{
   R_registerRoutines(info, NULL, callMethods, NULL, NULL);
   R_useDynamicSymbols(info, FALSE);
   R_forceSymbols(info, TRUE);
}
