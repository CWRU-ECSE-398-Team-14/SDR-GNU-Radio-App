#ifndef WATERFALL_H
#define WATERFALL_H

#include <QObject>
#include <QPixmap>
#include <QImageReader>
#include <cmath>

#define BMP_SIZE 8192
#define BMP_HEADER_SIZE 14
#define BMP_SIZE_IND    0x02
#define BMP_OFFSET_IND  0x0A

#define BMP_BITMAPCOREHEADER_SIZE 12
#define BMP_DIB_SIZE_IND    0x0E
#define BMP_DIB_WIDTH_IND   0x12
#define BMP_DIB_HEIGHT_IND  0x14
#define BMP_DIB_PLANES_IND  0x16
#define BMP_DIB_BITS_PER_PIX_IND    0x18

#define PIXEL_VALUE_MAX     16777216
#define PIXEL_VALUE_HALF    PIXEL_VALUE_MAX/2

typedef struct {
    uchar red;
    uchar green;
    uchar blue;
    uchar alpha;
}Pixel;

class Waterfall : public QObject
{
    Q_OBJECT
public:
    Waterfall(QObject *parent = nullptr, int width = 350, int maxHeight = 200);
    ~Waterfall();
    void setFFTMax(double max) { fftMax = max; fftHalf = fftMax/2.0; }

public slots:
    void appendFFT(const QVector<double>& fft);

signals:
    void pixmapReady(const QPixmap& pixmap);

private:
    void makeBmpHeader();
    void shiftRowsDown(int nRows);
    void addNewRow(double* values);
    void doubleToPixel(double value, uchar* pixData);
    int width;
    int maxHeight;
    int bmpFileSize;
    int pixelBytes;
    double fftMin = -50.0;
    double fftMax = 50.0;
    double fftHalf;
    double resolution = 100.0; // steps per dB
    uint8_t bpp = 32;
    uchar* bmp;
    double lutSize;
    Pixel* lut;
};

int nconstrain(int n, int min, int max);
double constrain(double x, double min, double max);
int nmap(int x, int in_min, int in_max, int out_min, int out_max);
double map(double x, double in_min, double in_max, double out_min, double out_max);
void scale(double* dest, int dest_size, double* src, int src_size);
int getBmpSize(uchar* bmp);
void setBmpSize(uchar* bmp, int size);

#endif // WATERFALL_H
