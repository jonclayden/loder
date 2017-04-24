#' @export
readPng <- function (file)
{
    .Call(C_read_png, file)
}

#' @export
writePng <- function (image, file)
{
    .Call(C_write_png, image, file)
}
