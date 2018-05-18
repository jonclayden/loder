context("Writing image data and metadata to PNG format")

test_that("we can write data and metadata to PNG format", {
    path <- system.file("extdata", "pngsuite", package="loder")
    images <- lapply(c("basn6a08.png","bgwn6a08.png","cdfn2c08.png","cdun2c08.png"), function(file) readPng(file.path(path,file)))
    temp <- tempfile()
    
    image <- readPng(writePng(images[[1]],temp))
    expect_equal(image[16,16,], c(32L,255L,4L,123L))
    image <- readPng(writePng(images[[1]],temp,interlace=TRUE))
    expect_equal(image[16,16,], c(32L,255L,4L,123L))
    image <- readPng(writePng(images[[1]],temp,range=c(0,127)))
    expect_equal(image[16,16,], c(64L,255L,8L,247L))
    
    image <- readPng(writePng(images[[2]],temp))
    expect_equal(attr(image,"background"), "#FFFFFF")
    
    image <- readPng(writePng(images[[3]],temp))
    expect_equal(attr(image,"asp"), 4)
    
    image <- readPng(writePng(images[[4]],temp))
    expect_equal(attr(image,"dpi"), c(25.4,25.4))
})
