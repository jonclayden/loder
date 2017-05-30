#define R_NO_REMAP

#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>

#include "lodepng.h"

SEXP read_png (SEXP file_)
{
    unsigned error;
    unsigned width, height, channels = 0;
    unsigned char *png = NULL, *data = NULL;
    size_t png_size;
    LodePNGState state;
    
    // Initialise the state object
    lodepng_state_init(&state);
    
    // Read the file into memory
    const char *filename = CHAR(STRING_ELT(file_, 0));
    error = lodepng_load_file(&png, &png_size, filename);
    if (error)
    {
        free(png);
        Rf_error("LodePNG error: %s\n", lodepng_error_text(error));
    }
    
    // Read basic metadata from the image blob
    error = lodepng_inspect(&width, &height, &state, png, png_size);
    if (error)
    {
        free(png);
        Rf_error("LodePNG error: %s\n", lodepng_error_text(error));
    }
    
    // Figure out the number of channels
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
    
    // Allocate memory for the final image
    R_len_t length;
    SEXP image, dim, range, class, asp, dpi, pixdim;
    char background[8] = "";
    length = (R_len_t) width * height * channels;
    PROTECT(image = Rf_allocVector(INTSXP,length));
    
    // Set the required colour type and bit depth, and decode the blob
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
    
    // Set the image dimensions
    PROTECT(dim = Rf_allocVector(INTSXP,3));
    int *dim_ptr = INTEGER(dim);
    dim_ptr[0] = height;
    dim_ptr[1] = width;
    dim_ptr[2] = channels;
    Rf_setAttrib(image, R_DimSymbol, dim);
    
    // Set the object class
    PROTECT(class = Rf_allocVector(STRSXP,2));
    SET_STRING_ELT(class, 0, Rf_mkChar("loder"));
    SET_STRING_ELT(class, 1, Rf_mkChar("array"));
    Rf_setAttrib(image, R_ClassSymbol, class);
    
    // Set the theoretical range of the data
    PROTECT(range = Rf_allocVector(INTSXP,2));
    INTEGER(range)[0] = 0;
    INTEGER(range)[1] = 255;
    Rf_setAttrib(image, Rf_install("range"), range);
    
    // If a background colour is defined in the file, convert it to a hex code
    // and store it
    if (state.info_png.background_defined)
    {
        sprintf(background, "#%X%X%X", state.info_png.background_r, state.info_png.background_g, state.info_png.background_b);
        Rf_setAttrib(image, Rf_install("background"), Rf_mkString(background));
    }
    
    // Set the aspect ratio or DPI/pixel size if available
    if (state.info_png.phys_defined)
    {
        if (state.info_png.phys_unit == 0)
        {
            PROTECT(asp = Rf_allocVector(REALSXP,1));
            *REAL(asp) = (double) state.info_png.phys_y / (double) state.info_png.phys_x;
            Rf_setAttrib(image, Rf_install("asp"), asp);
            UNPROTECT(1);
        }
        else
        {
            PROTECT(dpi = Rf_allocVector(REALSXP,2));
            PROTECT(pixdim = Rf_allocVector(REALSXP,2));
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
    
    // Tidy up
    lodepng_state_cleanup(&state);
    free(data);
    
    UNPROTECT(4);
    return image;
}

SEXP write_png (SEXP image_, SEXP file_, SEXP range_)
{
    unsigned width, height, channels;
    
    // Read the image dimensions from the source object
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
    
    // Check that the image data is numeric, then coerce it to double
    const int image_type = TYPEOF(image_);
    SEXP image;
    double *image_ptr;
    if (image_type != INTSXP && image_type != LGLSXP && image_type != REALSXP)
        Rf_error("Image data must be numeric or logical");
    PROTECT(image = Rf_coerceVector(image_, REALSXP));
    image_ptr = REAL(image);
    
    double min = R_PosInf, max = R_NegInf;
    Rboolean add_alpha = FALSE;
    R_len_t length = (R_len_t) width * height * channels;
    
    // Check for a range argument or attribute, or calculate range from data
    SEXP range;
    if (Rf_isNull(range_))
        range_ = Rf_getAttrib(image_, Rf_install("range"));
    if (!Rf_isNull(range_) && Rf_length(range_) == 2)
    {
        PROTECT(range = Rf_coerceVector(range_, REALSXP));
        double *range_ptr = REAL(range);
        min = (range_ptr[0] < range_ptr[1] ? range_ptr[0] : range_ptr[1]);
        max = (range_ptr[0] > range_ptr[1] ? range_ptr[0] : range_ptr[1]);
        UNPROTECT(1);
    }
    else if (image_type == LGLSXP)
    {
        min = 0.0;
        max = 255.0;
    }
    else
    {
        for (R_len_t l=0; l<length; l++)
        {
            const double value = ((double *) image_ptr)[l];
            if (ISNA(value))
            {
                if (!add_alpha && channels % 2 == 1)
                    add_alpha = TRUE;
            }
            else
            {
                if (value < min)
                    min = value;
                if (value > max)
                    max = value;
            }
        }
    }
    
    if (min == max)
        Rf_warning("Image is totally flat");
    
    // if (add_alpha)
    //     channels++;
    
    double range_width = max - min;
    unsigned error;
    unsigned char *png = NULL, *data;
    size_t png_size = (size_t) height * width * channels;
    LodePNGState state;
    
    // Convert to final unsigned char form, quantising as necessary
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
            {
                const double value = round((image_ptr[i+data_offset+k*image_stride] - min) / range_width * 255.0);
                if (value < 0.0)
                    *data_ptr++ = 0;
                else if (value > 255.0)
                    *data_ptr++ = 255;
                else
                    *data_ptr++ = (unsigned char) value;
            }
        }
    }
    
    // Initialise the state object
    lodepng_state_init(&state);
    
    // Set the final data representation
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
    
    // Check for a background attribute and attach it to the state if present
    SEXP background = Rf_getAttrib(image_, Rf_install("background"));
    if (!Rf_isNull(background))
    {
        long value = strtol(CHAR(STRING_ELT(background,0))+1, NULL, 16);
        if (value > 0)
        {
            state.info_png.background_defined = 1;
            state.info_png.background_r = (unsigned) (value & 0xff0000) >> 16;
            state.info_png.background_g = (unsigned) (value & 0x00ff00) >> 8;
            state.info_png.background_b = (unsigned) (value & 0x0000ff);
        }
    }
    
    // Check for a DPI or aspect ratio attribute (in that order of preference)
    SEXP dpi = Rf_getAttrib(image_, Rf_install("dpi"));
    SEXP asp = Rf_getAttrib(image_, Rf_install("asp"));
    if (!Rf_isNull(dpi) && Rf_length(dpi) == 2)
    {
        // Real units: convert DPI to pixels per metre
        state.info_png.phys_defined = 1;
        state.info_png.phys_unit = 1;
        state.info_png.phys_x = (unsigned) round(REAL(dpi)[0] * 39.3700787402);
        state.info_png.phys_y = (unsigned) round(REAL(dpi)[1] * 39.3700787402);
    }
    else if (!Rf_isNull(asp))
    {
        // Aspect ratio only: fix x size at 1000 and set y appropriately
        state.info_png.phys_defined = 1;
        state.info_png.phys_unit = 0;
        state.info_png.phys_x = 1000;
        state.info_png.phys_y = (unsigned) round(*REAL(asp) * 1000.0);
    }
    
    // Encode the data in memory
    const char *filename = CHAR(STRING_ELT(file_, 0));
    error = lodepng_encode(&png, &png_size, data, width, height, &state);
    if (error)
    {
        free(png);
        Rf_error("LodePNG error: %s\n", lodepng_error_text(error));
    }
    
    // Save to file
    lodepng_save_file(png, png_size, filename);
    if (error)
    {
        free(png);
        Rf_error("LodePNG error: %s\n", lodepng_error_text(error));
    }
    
    // Tidy up
    lodepng_state_cleanup(&state);
    free(png);
    
    UNPROTECT(1);
    return R_NilValue;
}

static R_CallMethodDef callMethods[] = {
    { "read_png",   (DL_FUNC) &read_png,    1 },
    { "write_png",  (DL_FUNC) &write_png,   3 },
    { NULL, NULL, 0 }
};

void R_init_loder (DllInfo *info)
{
   R_registerRoutines(info, NULL, callMethods, NULL, NULL);
   R_useDynamicSymbols(info, FALSE);
   R_forceSymbols(info, TRUE);
}
