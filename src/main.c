#define R_NO_REMAP

#include <R.h>
#include <Rdefines.h>
#include <R_ext/Rdynload.h>

#include "lodepng.h"

SEXP read_png (SEXP file_)
{
    unsigned error;
    unsigned width, height, channels;
    unsigned char *png = NULL, *data = NULL;
    size_t png_size;
    LodePNGState state;
    
    lodepng_state_init(&state);
    
    const char *filename = CHAR(STRING_ELT(file_, 0));
    error = lodepng_load_file(&png, &png_size, filename);
    if (error)
    {
        free(png);
        Rf_error("LodePNG error: %s\n", lodepng_error_text(error));
    }
    
    error = lodepng_inspect(&width, &height, &state, png, png_size);
    if (error)
    {
        free(png);
        Rf_error("LodePNG error: %s\n", lodepng_error_text(error));
    }
    
    switch (state.info_png.color.colortype)
    {
        case LCT_GREY:
        channels = 1;
        break;
        
        case LCT_GREY_ALPHA:
        channels = 2;
        break;
        
        case LCT_RGB:
        channels = 3;
        break;
        
        case LCT_PALETTE:
        case LCT_RGBA:
        channels = 4;
        break;
    }
    
    R_len_t length;
    SEXP image, dim, range, class, asp, dpi, pixdim;
    char background[8] = "";
    length = (R_len_t) width * height * channels;
    PROTECT(image = NEW_INTEGER(length));
    
    state.info_raw.colortype = (state.info_png.color.colortype == LCT_PALETTE ? LCT_RGBA : state.info_png.color.colortype);
    state.info_raw.bitdepth = 8;
    error = lodepng_decode(&data, &width, &height, &state, png, png_size);
    free(png);
    if (error)
    {
        free(data);
        Rf_error("LodePNG error: %s\n", lodepng_error_text(error));
    }
    
    // LodePNG returns pixel data with dimensions reversed relative to R, so we need to correct it back
    int *image_ptr = INTEGER(image);
    size_t image_strides[2] = { (size_t) height, (size_t) height * width };
    size_t data_strides[2] = { (size_t) channels, (size_t) channels * width };
    size_t image_offset, data_offset;
    for (unsigned i=0; i<height; i++)
    {
        data_offset = i * data_strides[1];
        for (unsigned j=0; j<width; j++)
        {
            image_offset = j * image_strides[0];
            for (unsigned k=0; k<channels; k++)
                image_ptr[i+image_offset+k*image_strides[1]] = (int) data[k+data_offset];
            data_offset += data_strides[0];
        }
    }
    
    PROTECT(dim = NEW_INTEGER(3));
    int *dim_ptr = INTEGER(dim);
    dim_ptr[0] = height;
    dim_ptr[1] = width;
    dim_ptr[2] = channels;
    Rf_setAttrib(image, R_DimSymbol, dim);
    
    PROTECT(class = NEW_CHARACTER(2));
    SET_STRING_ELT(class, 0, Rf_mkChar("loder"));
    SET_STRING_ELT(class, 1, Rf_mkChar("array"));
    Rf_setAttrib(image, R_ClassSymbol, class);
    
    PROTECT(range = NEW_INTEGER(2));
    INTEGER(range)[0] = 0;
    INTEGER(range)[1] = 255;
    Rf_setAttrib(image, Rf_install("range"), range);
    
    if (state.info_png.background_defined)
    {
        sprintf(background, "#%X%X%X", state.info_png.background_r, state.info_png.background_g, state.info_png.background_b);
        Rf_setAttrib(image, Rf_install("background"), Rf_mkString(background));
    }
    
    if (state.info_png.phys_defined)
    {
        if (state.info_png.phys_unit == 0)
        {
            PROTECT(asp = NEW_NUMERIC(1));
            *REAL(asp) = (double) state.info_png.phys_y / (double) state.info_png.phys_x;
            Rf_setAttrib(image, Rf_install("asp"), asp);
            UNPROTECT(1);
        }
        else
        {
            PROTECT(dpi = NEW_NUMERIC(2));
            PROTECT(pixdim = NEW_NUMERIC(2));
            REAL(dpi)[0] = (double) state.info_png.phys_x / 39.3700787402;
            REAL(dpi)[1] = (double) state.info_png.phys_y / 39.3700787402;
            REAL(pixdim)[0] = 1000.0 / (double) state.info_png.phys_x;
            REAL(pixdim)[1] = 1000.0 / (double) state.info_png.phys_y;
            Rf_setAttrib(image, Rf_install("dpi"), dpi);
            Rf_setAttrib(image, Rf_install("pixdim"), pixdim);
            Rf_setAttrib(image, Rf_install("pixunits"), Rf_mkString("mm"));
            UNPROTECT(2);
        }
    }
    
    lodepng_state_cleanup(&state);
    free(data);
    
    UNPROTECT(4);
    return image;
}

SEXP write_png (SEXP image_, SEXP file_)
{
    unsigned width, height, channels;
    
    SEXP dim = Rf_getAttrib(image_, R_DimSymbol);
    if (Rf_isNull(dim))
        Rf_error("Image does not have a \"dim\" attribute");
    int *dim_ptr = INTEGER(dim);
    height = dim_ptr[0];
    width = dim_ptr[1];
    if (Rf_length(dim) < 3)
        channels = 1;
    else
        channels = dim_ptr[2];
    
    void *image_ptr;
    const int image_type = TYPEOF(image_);
    if (image_type == INTSXP || image_type == LGLSXP)
        image_ptr = (void *) INTEGER(image_);
    else if (image_type == REALSXP)
        image_ptr = (void *) REAL(image_);
    else
        Rf_error("Image data must be numeric or logical");
    
    
    double min = R_PosInf, max = R_NegInf;
    Rboolean add_alpha = FALSE;
    R_len_t length = (R_len_t) width * height * channels;
    double *dbl_data = (double *) R_alloc(length, sizeof(double));
    double *dbl_data_ptr = dbl_data;
    
    for (R_len_t l=0; l<length; l++)
    {
        if (image_type == LGLSXP)
        {
            const int value = ((int *) image_ptr)[l];
            const Rboolean is_na = (value == NA_LOGICAL);
            if (is_na)
            {
                if (!add_alpha && channels % 2 == 1)
                    add_alpha = TRUE;
                *dbl_data_ptr++ = NA_REAL;
            }
            else
                *dbl_data_ptr++ = (double) value;
        }
        else if (image_type == REALSXP)
        {
            const double value = ((double *) image_ptr)[l];
            const Rboolean is_na = ISNA(value);
            if (is_na)
            {
                if (!add_alpha && channels % 2 == 1)
                    add_alpha = TRUE;
                *dbl_data_ptr++ = NA_REAL;
            }
            else
            {
                if (value < min)
                    min = value;
                if (value > max)
                    max = value;
                *dbl_data_ptr++ = value;
            }
        }
        else
        {
            const int value = ((int *) image_ptr)[l];
            const Rboolean is_na = (value == NA_INTEGER);
            if (is_na)
            {
                if (!add_alpha && channels % 2 == 1)
                    add_alpha = TRUE;
                *dbl_data_ptr++ = NA_REAL;
            }
            else
            {
                if ((double) value < min)
                    min = (double) value;
                if ((double) value > max)
                    max = (double) value;
                *dbl_data_ptr++ = (double) value;
            }
        }
    }
    
    if (image_type == LGLSXP)
    {
        min = 0.0;
        max = 255.0;
    }
    else if (min != R_PosInf && max == R_NegInf)
    {
        max = min;
        Rf_warning("Image is totally flat");
    }
    
    // if (add_alpha)
    //     channels++;
    
    double range = max - min;
    unsigned error;
    unsigned char *png = NULL, *data;
    size_t png_size = (size_t) height * width * channels;
    LodePNGState state;
    
    data = (unsigned char *) R_alloc(png_size, 1);
    unsigned char *data_ptr = data;
    size_t image_stride = (size_t) height * width;
    size_t data_offset;
    for (unsigned i=0; i<height; i++)
    {
        for (unsigned j=0; j<width; j++)
        {
            data_offset = j * height;
            for (unsigned k=0; k<channels; k++)
                *data_ptr++ = (unsigned char) round((dbl_data[i+data_offset+k*image_stride] - min) / range * 255.0);
        }
    }
    
    lodepng_state_init(&state);
    
    switch (channels)
    {
        case 1:
        state.info_raw.colortype = LCT_GREY;
        break;
        
        case 2:
        state.info_raw.colortype = LCT_GREY_ALPHA;
        break;
        
        case 3:
        state.info_raw.colortype = LCT_RGB;
        break;
        
        case 4:
        state.info_raw.colortype = LCT_RGBA;
        break;
    }
    
    const char *filename = CHAR(STRING_ELT(file_, 0));
    error = lodepng_encode(&png, &png_size, data, width, height, &state);
    if (error)
    {
        free(png);
        Rf_error("LodePNG error: %s\n", lodepng_error_text(error));
    }
    
    lodepng_save_file(png, png_size, filename);
    if (error)
    {
        free(png);
        Rf_error("LodePNG error: %s\n", lodepng_error_text(error));
    }
    
    lodepng_state_cleanup(&state);
    free(png);
    
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
