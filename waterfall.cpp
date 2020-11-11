#include "waterfall.h"

/**
 * @brief Waterfall::Waterfall constructor
 * @param parent used for QObject
 * @param width width in pixels of the waterfall
 * @param maxHeight height in pixels of the waterfall
 */
Waterfall::Waterfall(QObject *parent, int width, int maxHeight) :
    QObject(parent)
{
    this->width = width;
    this->maxHeight = maxHeight;
    this->fftHalf = (this->fftMin + this->fftMax)/2.0;

    int bytes_per_row = ((this->bpp*this->width + 31)/32)*4;
    this->pixelBytes = bytes_per_row*this->maxHeight;
    this->bmpFileSize = BMP_HEADER_SIZE + BMP_BITMAPCOREHEADER_SIZE + this->pixelBytes;

    this->bmp = (uchar*)malloc(this->bmpFileSize); // allocate memory for our BMP "file"

    this->makeBmpHeader();

    // create our power-to-pixel-value look-up-table
    lutSize = (fftMax - fftMin)*resolution + 1;
    this->lut = (Pixel*)malloc(lutSize * sizeof(Pixel));
    double pwr = fftMin; // pwr in dBm
    for(int i = 0; i < lutSize; i++){
        // start with blue
        double blue = fftHalf - pwr;
        blue = constrain(blue, 0.0, fftHalf - fftMin);

        // now green
        double green = fftHalf - fabs(fftHalf - pwr);
        green = constrain(green, 0.0, fftHalf - fabs(fftHalf));

        // lastly red
        double red = pwr - fftHalf;
        red = constrain(red, 0.0, fftMax - fftHalf);

        lut[i].blue = uchar(map(blue, 0.0, fftHalf - fftMin, 0.0, 255.0) + 0.5);
        lut[i].green = uchar(map(green, 0.0, fftHalf - fabs(fftHalf), 0.0, 255.0) + 0.5);
        lut[i].red = uchar(map(red, 0.0, fftMax - fftHalf, 0.0, 255.0) + 0.5);
        lut[i].alpha = 0;

        pwr += 1.0/resolution;
    }
}


Waterfall::~Waterfall(){
    free(this->bmp);
    free(this->lut);
}

/**
 * @brief Waterfall::appendFFT slot for external process to add new fft data
 * @param fft
 * will emit the pixmapReady signal when bitmap is formed
 */
void Waterfall::appendFFT(const QVector<double>& fft){

    // shift all existing pixels down
    this->shiftRowsDown(1);

    if(fft.size() != this->width){
        double* temp = (double*)malloc(this->width*sizeof(double));
        memset(temp, 0, this->width*sizeof(double));
        scale(temp, this->width, (double*)fft.data(), fft.size());
        this->addNewRow(temp);
        free(temp);
    }else{
        this->addNewRow((double*)fft.data());
    }

    QPixmap pixmap;
    pixmap.loadFromData(this->bmp, getBmpSize(this->bmp), QImageReader::supportedImageFormats()[0]);

    emit pixmapReady(pixmap);
}


/**
 * @brief Waterfall::makeBmpHeader make the bmp header based on width and height parameters
 */
void Waterfall::makeBmpHeader(){
    this->bmp[0] = 0x42;
    this->bmp[1] = 0x4D;
    setBmpSize(this->bmp, this->bmpFileSize); // set file size
    this->bmp[0x06] = 0;
    this->bmp[0x07] = 0;
    this->bmp[0x08] = 0;
    this->bmp[0x09] = 0;
    this->bmp[BMP_OFFSET_IND] = BMP_HEADER_SIZE + BMP_BITMAPCOREHEADER_SIZE; // skip file header and DIB header

    this->bmp[BMP_DIB_SIZE_IND] = BMP_BITMAPCOREHEADER_SIZE;
    this->bmp[BMP_DIB_WIDTH_IND] = uint16_t(this->width) & 0xFF;
    this->bmp[BMP_DIB_WIDTH_IND+1] = (uint16_t(this->width) >> 8) & 0xFF;
    this->bmp[BMP_DIB_HEIGHT_IND] = uint16_t(this->maxHeight) & 0xFF;
    this->bmp[BMP_DIB_HEIGHT_IND+1] = (uint16_t(this->maxHeight) >> 8) & 0xFF;
    this->bmp[BMP_DIB_PLANES_IND] = 1; // this must always be 1 according to wikipedia
    this->bmp[BMP_DIB_BITS_PER_PIX_IND] = this->bpp;
    this->bmp[BMP_DIB_BITS_PER_PIX_IND+1] = 0;

}

/**
 * @brief Waterfall::shiftRowsDown shift pixels in bmp "down" by nRows
 * @param nRows rows to shift down
 */
void Waterfall::shiftRowsDown(int nRows){
    int delta = nRows*this->width*(this->bpp/8);
    uchar* dest = this->bmp + BMP_HEADER_SIZE + BMP_BITMAPCOREHEADER_SIZE;
    uchar* src  = this->bmp + BMP_HEADER_SIZE + BMP_BITMAPCOREHEADER_SIZE + delta;
    int size = this->pixelBytes - delta;    // how much memory we moving?
    memmove(dest, src, size);
}

/**
 * @brief Waterfall::addNewRow converts values into pixel data and places it at the top of the bitmap (end of memory block)
 * @param values fft values to be converted into pixel data
 * assumes values is at least as large as the image width
 */
void Waterfall::addNewRow(double* values){
    int bytes_per_pix = this->bpp/8;
    uchar* pix_start = this->bmp + (this->bmpFileSize - bytes_per_pix*this->width);
    for(int i = 0; i < this->width; i++){
        this->doubleToPixel(values[i], pix_start + bytes_per_pix*i);
    }
}

/**
 * @brief Waterfall::doubleToPixel converts a double into a single pixel
 * @param value the value
 * @param pixData the memory location of the pixel
 */
void Waterfall::doubleToPixel(double value, uchar* pixData){

    int ind = nconstrain(int(0.5 + (value - fftMin)*resolution), 0, lutSize - 1);

    // blue red green alpha
    pixData[0] = lut[ind].red;
    pixData[1] = lut[ind].green;
    pixData[2] = lut[ind].blue;
    pixData[3] = lut[ind].alpha;

}

int nconstrain(int n, int min, int max){
    return (n < min ? min : (n > max ? max : n));
}

double constrain(double x, double min, double max){
    return (x < min ? min : (x > max ? max : x));
}

int nmap(int x, int in_min, int in_max, int out_min, int out_max){
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

double map(double x, double in_min, double in_max, double out_min, double out_max){
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/**
 * @brief scale scales array src to array dest with minimal losses
 * @param dest starts as an array of 0's, will be populated with scaled values computed from src
 * @param dest_size size of dest
 * @param src source data to fit into dest
 * @param src_size size of src
 */
void scale(double* dest, int dest_size, double* src, int src_size){
    int temp_size = dest_size*src_size;

    double weight_1 = double(src_size)/double(temp_size);
    double weight_2 = double(dest_size)/double(src_size); // scaling from src to dest
    double factor = double(temp_size)/double(dest_size); // to get the right dest index

    for(int i = 0; i < temp_size; i++){
        dest[int(i/factor)] += weight_2 * weight_1 * src[ int(i*weight_1) ];
    }

}

/**
 * @brief getBmpSize reads the bmp size from the header
 * @param bmp the bitmap "file"
 * @return size read from the header
 */
int getBmpSize(uchar* bmp){
    return (bmp[BMP_SIZE_IND] | (bmp[BMP_SIZE_IND+1] << 8) | (bmp[BMP_SIZE_IND+2] << 16) | (bmp[BMP_SIZE_IND+3] << 24));
}

/**
 * @brief setBmpSize sets the bmp size header field to size
 * @param bmp the "file"
 * @param size the size of the "file" in bytes
 */
void setBmpSize(uchar* bmp, int size){
    bmp[BMP_SIZE_IND] = size & 0xFF;
    bmp[BMP_SIZE_IND+1] = (size >> 8) & 0xFF;
    bmp[BMP_SIZE_IND+2] = (size >> 16) & 0xFF;
    bmp[BMP_SIZE_IND+3] = (size >> 24) & 0xFF;
}
