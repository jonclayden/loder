#define R_NO_REMAP

#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>

#include "lodepng.h"

// Predefined compression levels
// Elements are block type, use LZ77, window size, minimum LZ77 length, threshold length to stop searching, use lazy matching
// Last three elements are for custom hooks and are not used here
const LodePNGCompressSettings level0 = { 0, 0,  2048, 3,  16, 0, 0, 0, 0 };  // No compression
const LodePNGCompressSettings level1 = { 1, 1,   256, 3,  32, 1, 0, 0, 0 };  // Fixed Huffman tree, small window
const LodePNGCompressSettings level2 = { 1, 1,  1024, 3,  64, 1, 0, 0, 0 };  // Fixed Huffman tree, medium window
const LodePNGCompressSettings level3 = { 2, 1,  1024, 3,  64, 1, 0, 0, 0 };  // Dynamic tree, medium window
const LodePNGCompressSettings level4 = { 2, 1,  2048, 3, 128, 1, 0, 0, 0 };  // LodePNG defaults
const LodePNGCompressSettings level5 = { 2, 1,  8192, 3, 128, 1, 0, 0, 0 };  // Dynamic tree, large window
const LodePNGCompressSettings level6 = { 2, 1, 32768, 3, 258, 1, 0, 0, 0 };  // Maximum compression

SEXP read_png (SEXP file_, SEXP require_data_)
{
    const Rboolean require_data = (Rf_asLogical(require_data_) == TRUE);
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
    SEXP image, dim, class, asp, dpi, pixdim, text_keys, text_vals;
    char background[8] = "";
    length = (R_len_t) width * height * channels;
    
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
    
    if (require_data)
    {
        // LodePNG returns pixel data with dimensions reversed relative to R, so we need to correct it back
        PROTECT(image = Rf_allocVector(INTSXP,length));
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
        SEXP range;
        PROTECT(range = Rf_allocVector(INTSXP,2));
        INTEGER(range)[0] = 0;
        INTEGER(range)[1] = 255;
        Rf_setAttrib(image, Rf_install("range"), range);
        
        UNPROTECT(3);
    }
    else
    {
        // We don't need the data, so just return the input vector with attributes
        PROTECT(image = Rf_duplicate(file_));
        
        // Set the object class and other basic attributes
        Rf_setAttrib(image, R_ClassSymbol, PROTECT(Rf_mkString("lodermeta")));
        Rf_setAttrib(image, Rf_install("width"), PROTECT(Rf_ScalarInteger(width)));
        Rf_setAttrib(image, Rf_install("height"), PROTECT(Rf_ScalarInteger(height)));
        Rf_setAttrib(image, Rf_install("channels"), PROTECT(Rf_ScalarInteger(channels)));
        Rf_setAttrib(image, Rf_install("bitdepth"), PROTECT(Rf_ScalarInteger(state.info_png.color.bitdepth)));
        Rf_setAttrib(image, Rf_install("filesize"), PROTECT(Rf_ScalarReal((double) png_size)));
        Rf_setAttrib(image, Rf_install("interlaced"), PROTECT(Rf_ScalarLogical(state.info_png.interlace_method)));
        
        // If a palette is used, capture the number of colours
        if (state.info_png.color.colortype == LCT_PALETTE)
        {
            Rf_setAttrib(image, Rf_install("palette"), PROTECT(Rf_ScalarInteger((int) state.info_png.color.palettesize)));
            UNPROTECT(1);
        }
        
        UNPROTECT(7);
    }
    
    // If a background colour is defined in the file, convert it to a hex code and store it
    if (state.info_png.background_defined)
    {
        sprintf(background, "#%X%X%X", state.info_png.background_r, state.info_png.background_g, state.info_png.background_b);
        Rf_setAttrib(image, Rf_install("background"), PROTECT(Rf_mkString(background)));
        UNPROTECT(1);
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
            Rf_setAttrib(image, Rf_install("pixunits"), PROTECT(Rf_mkString("mm")));
            UNPROTECT(3);
        }
    }
    
    // Convert text chunks
    if (state.info_png.itext_num > 0)
    {
        PROTECT(text_keys = Rf_allocVector(STRSXP, state.info_png.itext_num + state.info_png.text_num));
        PROTECT(text_vals = Rf_allocVector(STRSXP, state.info_png.itext_num + state.info_png.text_num));
        
        for (size_t i=0; i<state.info_png.itext_num; i++)
        {
            SET_STRING_ELT(text_keys, i, Rf_mkCharCE(state.info_png.itext_transkeys[i], CE_UTF8));
            SET_STRING_ELT(text_vals, i, Rf_mkCharCE(state.info_png.itext_strings[i], CE_UTF8));
        }
        for (size_t i=0; i<state.info_png.text_num; i++)
        {
            SET_STRING_ELT(text_keys, i+state.info_png.itext_num, Rf_mkChar(state.info_png.text_keys[i]));
            SET_STRING_ELT(text_vals, i+state.info_png.itext_num, Rf_mkChar(state.info_png.text_strings[i]));
        }
        
        Rf_setAttrib(text_vals, R_NamesSymbol, text_keys);
        Rf_setAttrib(image, Rf_install("text"), text_vals);
        UNPROTECT(2);
    }
    else if (state.info_png.text_num > 0)
    {
        PROTECT(text_keys = Rf_allocVector(STRSXP, state.info_png.text_num));
        PROTECT(text_vals = Rf_allocVector(STRSXP, state.info_png.text_num));
        
        for (size_t i=0; i<state.info_png.text_num; i++)
        {
            SET_STRING_ELT(text_keys, i, Rf_mkChar(state.info_png.text_keys[i]));
            SET_STRING_ELT(text_vals, i, Rf_mkChar(state.info_png.text_strings[i]));
        }
        
        Rf_setAttrib(text_vals, R_NamesSymbol, text_keys);
        Rf_setAttrib(image, Rf_install("text"), text_vals);
        UNPROTECT(2);
    }
    
    // Tidy up
    lodepng_state_cleanup(&state);
    free(data);
    
    UNPROTECT(1);
    return image;
}

SEXP write_png (SEXP image_, SEXP file_, SEXP compression_level_, SEXP interlace_)
{
    const int compression_level = Rf_asInteger(compression_level_);
    const Rboolean interlace = (Rf_asLogical(interlace_) == TRUE);
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
    
    // Check for a range attribute, or calculate from data
    SEXP range = Rf_getAttrib(image_, Rf_install("range"));
    if (!Rf_isNull(range) && Rf_length(range) == 2)
    {
        SEXP dbl_range;
        PROTECT(dbl_range = Rf_coerceVector(range, REALSXP));
        double *range_ptr = REAL(dbl_range);
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
        case 1: state.info_raw.colortype = LCT_GREY;        break;
        case 2: state.info_raw.colortype = LCT_GREY_ALPHA;  break;
        case 3: state.info_raw.colortype = LCT_RGB;         break;
        case 4: state.info_raw.colortype = LCT_RGBA;        break;
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
    
    // Check for a text attribute
    SEXP text_vals = Rf_getAttrib(image_, Rf_install("text"));
    if (!Rf_isNull(text_vals))
    {
        const int text_length = Rf_length(text_vals);
        SEXP text_keys = Rf_getAttrib(text_vals, R_NamesSymbol);
        const Rboolean have_keys = !Rf_isNull(text_keys);
        Rboolean warned = FALSE;
        for (int i=0; i<text_length; i++)
        {
            const SEXP string = STRING_ELT(text_vals, i);
            const cetype_t key_encoding = have_keys ? Rf_getCharCE(STRING_ELT(text_keys,i)) : CE_NATIVE;
            const cetype_t string_encoding = Rf_getCharCE(string);
            
            if (key_encoding == CE_NATIVE && string_encoding == CE_NATIVE)
                lodepng_add_text(&state.info_png, have_keys ? CHAR(STRING_ELT(text_keys,i)) : "Comment", CHAR(string));
            else if (key_encoding == CE_UTF8 || string_encoding == CE_UTF8)
                lodepng_add_itext(&state.info_png, "Comment", "", have_keys ? CHAR(STRING_ELT(text_keys,i)) : "Comment", CHAR(string));
            else if (!warned)
            {
                Rf_warning("Text element with non-UTF-8 encoding ignored");
                warned = TRUE;
            }
        }
    }
    
    state.info_png.interlace_method = interlace;
    
    switch (compression_level)
    {
        case 0: state.encoder.zlibsettings = level0; break;
        case 1: state.encoder.zlibsettings = level1; break;
        case 2: state.encoder.zlibsettings = level2; break;
        case 3: state.encoder.zlibsettings = level3; break;
        case 4: state.encoder.zlibsettings = level4; break;
        case 5: state.encoder.zlibsettings = level5; break;
        case 6: state.encoder.zlibsettings = level6; break;
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
    { "read_png",   (DL_FUNC) &read_png,    2 },
    { "write_png",  (DL_FUNC) &write_png,   4 },
    { NULL, NULL, 0 }
};

void R_init_loder (DllInfo *info)
{
   R_registerRoutines(info, NULL, callMethods, NULL, NULL);
   R_useDynamicSymbols(info, FALSE);
   R_forceSymbols(info, TRUE);
}
