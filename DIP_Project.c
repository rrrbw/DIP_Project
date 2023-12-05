#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <emmintrin.h>
#include <immintrin.h>

// Function declarations
void Gaussian_Blur();
void Sobel();
void process_all_images();
void read_image(const wchar_t* filename);
void write_image2(const wchar_t* filename, unsigned char* output_image);
void openfile(const wchar_t* filename, FILE** finput);
int getint(FILE* fp);

#define INPUT_FOLDER L"D:\\image_processing\\input_images"
#define OUTPUT_FOLDER L"D:\\image_processing\\output_images"

// Image dimensions
int M;  // cols
int N;  // rows

// Arrays defined dynamically
unsigned char* frame1 = new unsigned char[1];  // Placeholder, will be resized dynamically
unsigned char* filt = new unsigned char[1];    // Placeholder, will be resized dynamically
unsigned char* gradient = new unsigned char[1]; // Placeholder, will be resized dynamically

const signed char Mask[5][5] = {
    {2, 4, 5, 4, 2},
    {4, 9, 12, 9, 4},
    {5, 12, 15, 12, 5},
    {4, 9, 12, 9, 4},
    {2, 4, 5, 4, 2}
};

const signed char GxMask[3][3] = {
    {-1, 0, 1},
    {-2, 0, 2},
    {-1, 0, 1}
};

const signed char GyMask[3][3] = {
    {-1, -2, -1},
    {0, 0, 0},
    {1, 2, 1}
};

char header[100];
errno_t err;

int main() {
    process_all_images();

    // Free dynamically allocated memory
    delete[] frame1;
    delete[] filt;
    delete[] gradient;

    return 0;
}

void process_all_images() {
    WIN32_FIND_DATAW findFileData;
    HANDLE hFind = FindFirstFileW(L"D:\\image_processing\\input_images\\*", &findFileData);

    if (hFind == INVALID_HANDLE_VALUE) {
        perror("Error opening directory");
        exit(EXIT_FAILURE);
    }

    do {
        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Skip directories
            continue;
        }
        wchar_t inputPath[256];
        wchar_t outputPathBlur[256];
        wchar_t outputPathEdge[256];

        // Build the full path by excluding the wildcard character
        swprintf_s(inputPath, sizeof(inputPath) / sizeof(inputPath[0]), L"%s\\%s", INPUT_FOLDER, findFileData.cFileName);
        swprintf_s(outputPathBlur, sizeof(outputPathBlur) / sizeof(outputPathBlur[0]), L"%s\\blurred_%s", OUTPUT_FOLDER, findFileData.cFileName);
        swprintf_s(outputPathEdge, sizeof(outputPathEdge) / sizeof(outputPathEdge[0]), L"%s\\edge_%s", OUTPUT_FOLDER, findFileData.cFileName);

        // Print the paths for debugging
        wprintf(L"Input Path: %s\n", inputPath);
        wprintf(L"Output Path Blur: %s\n", outputPathBlur);
        wprintf(L"Output Path Edge: %s\n", outputPathEdge);

        // Read, process, and write the images
        read_image(inputPath);
        Gaussian_Blur();
        Sobel();
        write_image2(outputPathBlur, filt);
        write_image2(outputPathEdge, gradient);

    } while (FindNextFileW(hFind, &findFileData) != 0);

    FindClose(hFind);
}

void Gaussian_Blur() {
    int row, col, rowOffset, colOffset;
    int newPixel;
    unsigned char pix;
    const unsigned short int size = 2;

    for (row = 0; row < N; row++) {
        for (col = 0; col < M; col++) {
            newPixel = 0;
            for (rowOffset = -size; rowOffset <= size; rowOffset++) {
                for (colOffset = -size; colOffset <= size; colOffset++) {

                    if ((row + rowOffset < 0) || (row + rowOffset >= N) || (col + colOffset < 0) || (col + colOffset >= M))
                        pix = 0;
                    else
                        pix = frame1[M * (row + rowOffset) + col + colOffset];

                    newPixel += pix * Mask[size + rowOffset][size + colOffset];

                }
            }
            filt[M * row + col] = (unsigned char)(newPixel / 159);

        }
    }
}

void Sobel() {
    for (int row = 1; row < N - 1; row++) {
        for (int col = 1; col < M - 1; col += 16) {
            __m128i pixels = _mm_loadu_si128((__m128i*) & filt[M * row + col]);
            __m128i pix1 = _mm_unpacklo_epi8(pixels, _mm_setzero_si128());
            __m128i pix2 = _mm_unpackhi_epi8(pixels, _mm_setzero_si128());
            __m128i Gx = _mm_setzero_si128();
            __m128i Gy = _mm_setzero_si128();

            for (int i = 0; i < 3; i++) {
                __m128i maskValuesX = _mm_loadu_si128((__m128i*)GxMask[i]);
                __m128i maskValuesY = _mm_loadu_si128((__m128i*)GyMask[i]);

                Gx = _mm_add_epi16(Gx, _mm_maddubs_epi16(pix1, maskValuesX));
                Gx = _mm_add_epi16(Gx, _mm_maddubs_epi16(pix2, maskValuesX));
                Gy = _mm_add_epi16(Gy, _mm_maddubs_epi16(pix1, maskValuesY));
                Gy = _mm_add_epi16(Gy, _mm_maddubs_epi16(pix2, maskValuesY));
            }

            __m128i gradientStrength = _mm_add_epi16(_mm_abs_epi16(Gx), _mm_abs_epi16(Gy));
            gradientStrength = _mm_packus_epi16(gradientStrength, _mm_setzero_si128());
            _mm_storeu_si128((__m128i*) & gradient[M * row + col], gradientStrength);
        }
    }
}

void read_image(const wchar_t* filename) {
    int c;
    FILE* finput;
    int i, j, temp;

    wprintf(L"\nReading %s image from disk ...", filename);
    finput = NULL;
    openfile(filename, &finput);

    if ((header[0] == 'P') && (header[1] == '5')) {
        for (j = 0; j < N; j++) {
            for (i = 0; i < M; i++) {
                temp = getc(finput);
                frame1[M * j + i] = (unsigned char)temp;
            }
        }
    }

    else if ((header[0] == 'P') && (header[1] == '2')) {
        for (j = 0; j < N; j++) {
            for (i = 0; i < M; i++) {
                if (fscanf_s(finput, "%d", &temp, 20) == EOF)
                    exit(EXIT_FAILURE);

                frame1[M * j + i] = (unsigned char)temp;
            }
        }
    }
    else {
        wprintf(L"\nproblem with reading the image");
        exit(EXIT_FAILURE);
    }

    fclose(finput);
    wprintf(L"\nimage successfully read from disc\n");
}

void write_image2(const wchar_t* filename, unsigned char* output_image) {
    FILE* foutput;
    int i, j;

    wprintf(L"  Writing result to disk ...\n");

    if ((err = _wfopen_s(&foutput, filename, L"wb")) != 0) {
        fwprintf(stderr, L"Unable to open file %ls for writing\n", filename);
        exit(-1);
    }

    fwprintf(foutput, L"P2\n");
    fwprintf(foutput, L"%d %d\n", M, N);
    fwprintf(foutput, L"%d\n", 255);

    for (j = 0; j < N; ++j) {
        for (i = 0; i < M; ++i) {
            fwprintf(foutput, L"%3d ", output_image[M * j + i]);
            if (i % 32 == 31) fwprintf(foutput, L"\n");
        }
        if (M % 32 != 0) fwprintf(foutput, L"\n");
    }
    fclose(foutput);
}

void openfile(const wchar_t* filename, FILE** finput) {
    int x0, y0, x, aa;

    if ((err = _wfopen_s(finput, filename, L"rb")) != 0) {
        fwprintf(stderr, L"Unable to open file %ls for reading\n", filename);
        exit(-1);
    }

    aa = fscanf_s(*finput, "%s", header, 20);

    x0 = getint(*finput);
    y0 = getint(*finput);

    // Update M and N with read dimensions
    M = x0;
    N = y0;

    // Allocate memory for dynamically sized arrays
    frame1 = new unsigned char[M * N];
    filt = new unsigned char[M * N];
    gradient = new unsigned char[M * N];

    wprintf(L"\t header is %s, while x=%d,y=%d", header, x0, y0);

    x = getint(*finput); /* read and throw away the range info */
}

int getint(FILE* fp) {
    int c, i, firstchar;

    c = getc(fp);
    while (1) {
        if (c == '#') 
            char cmt[256], * sp;
            sp = cmt;  firstchar = 1;
            while (1) {
                c = getc(fp);
                if (firstchar && c == ' ') firstchar = 0;
                else {
                    if (c == '\n' || c == EOF) break;
                    if ((sp - cmt) < 250) *sp++ = c;
                }
            }
            *sp++ = '\n';
            *sp = '\0';
        }

        if (c == EOF) return 0;
        if (c >= '0' && c <= '9') break;

        c = getc(fp);
    }

    i = 0;
    while (1) {
        i = (i * 10) + (c - '0');
        c = getc(fp);
        if (c == EOF) return i;
        if (c < '0' || c>'9') break;
    }
    return i;
}