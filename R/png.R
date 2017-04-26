#' Read a PNG file
#' 
#' Read an image from a PNG file and convert the pixel data into an R array.
#' 
#' @param file A character string giving the file name to read from.
#' @return An array of class \code{"loder"}.
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
#' @param image An array containing the pixel data.
#' @param file A character string giving the file name to write to.
#' @param range An optional numeric 2-vector giving the extremes of the
#'   intensity window.
#' 
#' @export
writePng <- function (image, file, range = NULL)
{
    invisible (.Call(C_write_png, image, file, range))
}
