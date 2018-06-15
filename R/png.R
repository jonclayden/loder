#' Read metadata from a PNG file
#' 
#' Inspect a PNG file, returning parsed metadata relating to it.
#' 
#' The LodePNG library is used to parse the PNG file at the specified path.
#' The result is a string like the input, but of class \code{"lodermeta"} and
#' with several attributes set describing the file's contents. There is a
#' \code{print} method for these objects.
#' 
#' @param file A character string giving the file name to read from.
#' @param x An object of class \code{"lodermeta"}.
#' @param ... Additional arguments (which are ignored).
#' @return \code{inspectPng} returns a character vector of class
#'   \code{"lodermeta"}. The \code{print} method is called for its side-effect.
#' 
#' @examples
#' path <- system.file("extdata", "pngsuite", package="loder")
#' inspectPng(file.path(path, "basn6a08.png"))
#' 
#' @seealso \code{readPng} to read the pixel values.
#' 
#' @export
inspectPng <- function (file)
{
    .Call(C_read_png, path.expand(file), FALSE)
}

#' @rdname inspectPng
#' @export
print.lodermeta <- function (x, ...)
{
    size <- function (n) switch(which(n>=c(2^30,2^20,2^10,0))[1], paste(round(n/(2^30),2),"GiB"), paste(round(n/(2^20),2),"MiB"), paste(round(n/(2^10),2),"kiB"), paste(n,"B"))
    
    channelString <- switch(attr(x,"channels"), "grey", "grey + alpha", "RGB", "RGB + alpha")
    textLength <- length(attr(x, "text"))
    textNames <- names(attr(x, "text"))
    textString <- paste0("- ", textLength, ifelse(is.null(textNames) || all(textNames==""), " unnamed", " named"), " text chunk", ifelse(textLength==1,"","s"))
    totalBytes <- attr(x,"width") * attr(x,"height") * attr(x,"channels") * attr(x,"bitdepth") / 8
    lines <- c(paste0("PNG file: ", as.character(x)),
               paste0("- ", attr(x,"height"), " x ", attr(x,"width"), " pixels, ", ifelse(attr(x,"interlaced"),"interlaced, ",""), channelString, ifelse(attr(x,"palette")>0,paste(" (palette of",attr(x,"palette"),"colours)"))),
               paste0("- ", attr(x,"bitdepth"), " bpp (uncompressed data size ", size(totalBytes), "; file size is ", size(attr(x,"filesize")), ")"))
    
    if (!is.null(attr(x, "background")))
        lines <- c(lines, paste("- Background colour is", attr(x,"background")))
    if (!is.null(attr(x, "pixdim")))
        lines <- c(lines, paste0("- Pixel dimensions are ", signif(attr(x,"pixdim")[1],3), " x ", signif(attr(x,"pixdim")[2],3), " mm (", round(attr(x,"dpi")[1],2), " x ", round(attr(x,"dpi")[2],2), " dpi)"))
    else if (!is.null(attr(x, "asp")))
        lines <- c(lines, paste("- Aspect ratio is", signif(attr(x,"asp"),3), ": 1"))
    if (textLength > 0)
        lines <- c(lines, textString)
    
    cat(paste(lines, collapse="\n"))
    cat("\n")
}

#' Read a PNG file
#' 
#' Read an image from a PNG file and convert the pixel data into an R array.
#' 
#' The LodePNG library is used to read the PNG file at the specified path.
#' LodePNG can handle a wide variety of subformats and bit depths, but the
#' output of this function is currently standardised to an integer-mode array
#' with 8-bit range, i.e. between 0 and 255. Attributes specifying the
#' background colour, spatial resolution and/or aspect ratio are attached to
#' the result if this information is stored with the image.
#' 
#' @param file A character string giving the file name to read from.
#' @param x An object of class \code{"loder"}.
#' @param ... Additional arguments (which are ignored).
#' @return \code{readPng} returns an integer-mode array of class
#'   \code{"loder"}. The \code{print} method is called for its side-effect.
#' 
#' @examples
#' path <- system.file("extdata", "pngsuite", package="loder")
#' image <- readPng(file.path(path, "basn6a08.png"))
#' print(image)
#' attributes(image)
#' 
#' @seealso \code{inspectPng} to read only metadata from the file. In addition,
#'   the \code{readPNG} function in the venerable \code{png} package offers
#'   similar functionality to \code{readPng}, but relies on an external
#'   installation of libpng. By contrast, \code{loder} includes the LodePNG
#'   library.
#' 
#' @export
readPng <- function (file)
{
    .Call(C_read_png, path.expand(file), TRUE)
}

#' @rdname readPng
#' @export
print.loder <- function (x, ...)
{
    dim <- dim(x)
    cat(paste0("PNG image array: ", dim[1], " x ", dim[2], " pixels, ", switch(dim[3], "grey", "grey + alpha", "RGB", "RGB + alpha"), "\n"))
}

#' Write a PNG file
#' 
#' Write a numeric or logical array to a PNG file.
#'
#' The LodePNG library is used to write a PNG file at the specified path. The
#' source data should be of logical, integer or numeric mode. Metadata
#' attributes of the image will be stored where applicable, and may be
#' overwritten using named arguments. LodePNG will choose the bit depth of the
#' final image.
#' 
#' Attributes which are currently stored are as follows. In each case an
#' argument of the appropriate name can be used to override a value stored with
#' the image.
#' \describe{
#'   \item{\code{range}}{A numeric 2-vector giving the extremes of the
#'     intensity window, i.e. the black and white points. Values outside this
#'     range will be clipped.}
#'   \item{\code{background}}{A hexadecimal colour string giving the background
#'     colour.}
#'   \item{\code{dpi}}{A numeric 2-vector giving the dots-per-inch resolution
#'     of the image in each dimension.}
#'   \item{\code{asp}}{The aspect ratio of the image. Ignored if \code{dpi} is
#'     present and valid.}
#'   \item{\code{text}}{A character vector (possibly named) of text strings to
#'     store in the file. Only ASCII and UTF-8 encoded strings are currently
#'     supported.}
#' }
#' Dimensions are always taken from the image, and cannot be modified here.
#' 
#' @param image An array containing the pixel data.
#' @param file A character string giving the file name to write to.
#' @param ... Additional metadata elements, which override equivalently named
#'   attributes of \code{image}. See Details.
#' @param compression Compression level, an integer value between 0 (no
#'   compression, fastest) and 6 (maximum compression, slowest).
#' @param interlace Logical value: should the image be interlaced?
#' @return The \code{file} argument, invisibly.
#' 
#' @seealso \code{\link{readPng}} for reading images.
#' 
#' @export
writePng <- function (image, file, ..., compression = 4L, interlace = FALSE)
{
    .Call(C_write_png, structure(image,...), path.expand(file), as.integer(compression), interlace)
    invisible(file)
}
