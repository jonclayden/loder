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
#' @return An integer-mode array of class \code{"loder"}.
#' 
#' @examples
#' path <- system.file("extdata", "pngsuite", package="loder")
#' image <- readPng(file.path(path, "basn6a08.png"))
#' attributes(image)
#' 
#' @seealso The \code{readPNG} function in the venerable \code{png} package
#'   offers similar functionality, but relies on an external installation of
#'   libpng. By contrast, \code{loder} includes the LodePNG library.
#' 
#' @export
readPng <- function (file)
{
    .Call(C_read_png, file)
}

#' Write a PNG file
#' 
#' Write a numeric or logical array to a PNG file.
#'
#' The LodePNG library is used to write a PNG file at the specified path. The
#' source data should be of logical, integer or numeric mode. The allowable
#' range of the data will be taken from the \code{range} argument if it is
#' specified, or from a \code{range} attribute, if there is one, and otherwise
#' it is calculated from the data. Background colour, spatial resolution and/or
#' aspect ratio metadata are written to the file if the corresponding
#' attributes exist. LodePNG will choose the bit depth of the final image.
#' 
#' @param image An array containing the pixel data.
#' @param file A character string giving the file name to write to.
#' @param range An optional numeric 2-vector giving the extremes of the
#'   intensity window. Values outside this range will be clipped.
#' @return The \code{file} argument, invisibly.
#' 
#' @seealso \code{\link{readPng}} for reading images.
#' 
#' @export
writePng <- function (image, file, range = NULL)
{
    .Call(C_write_png, image, file, range)
    invisible(file)
}
