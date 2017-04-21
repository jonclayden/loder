#' @export
readPng <- function (file)
{
    .Call(C_read_png, file)
}

writePng <- function (image, file)
{
}
