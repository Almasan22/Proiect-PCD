#ifndef ENCODE_DECODE_IMAGE_H
#define ENCODE_DECODE_IMAGE_H

#include <stdio.h>    
#include <stdlib.h>   
#include <string.h>   
#include <zlib.h>  // Functii de compresie si decompresie (zlib)
#include <zip.h>   // Functii pentru lucrul cu arhive zip
#include <unistd.h>   
#include <dirent.h>
#include <sys/stat.h> 
#include <sys/types.h> 

// Marimea unui bloc de date (16 KB) utilizat la compresie/decompresie
#define CHUNK 16384

// Coduri de eroare si succes pentru functii
#define IMAGE_ENCODED_SUCCESFULY 1
#define NOT_VALID_BMP -1
#define MESSAGE_TOO_LONG -2
#define ERROR_CREATING_ENCODED_IMAGE -3
#define ERROR_OPENING_FILE -10

// Coduri de eroare si succes pentru operatiile legate de arhivare ZIP
#define ERROR_CREATING_ZIP_FILE -1
#define ZIP_SUCCES 1
#define ERR_LEN 256

// Structura pentru header-ul fisierului BMP
#pragma pack(push, 1) // Asigura ca structurile sunt aliniate pe 1 byte
typedef struct {
    unsigned short bfType;       // Tipul fisierului
    unsigned int bfSize;         // Dimensiunea fisierului in octeti
    unsigned short bfReserved1;  // Rezervat, trebuie sa fie 0
    unsigned short bfReserved2;  // Rezervat, trebuie sa fie 0
    unsigned int bfOffBits;      // Offset la datele imaginii
} BITMAPFILEHEADER;


// Structura cu info detaliate despre fisierului BMP
typedef struct {
    unsigned int biSize;           // Dimensiunea acestei structuri
    int biWidth;                   // Latimea imaginii
    int biHeight;                  // Inaltimea imaginii
    unsigned short biPlanes;       // Numarul de planuri
    unsigned short biBitCount;     // Numarul de biti per pixel
    unsigned int biCompression;    // Tipul de compresie
    unsigned int biSizeImage;      // Dimensiunea imaginii in octeti
    int biXPelsPerMeter;           // Rezolutia orizontala
    int biYPelsPerMeter;           // Rezolutia verticala
    unsigned int biClrUsed;        // Numarul de culori utilizate
    unsigned int biClrImportant;   // Numarul de culori importante
} BITMAPINFOHEADER;
#pragma pack(pop) // Reset aliniere

// Declaratiile functiilor
int encode_image(const char *image_path, const char *message, int client_id);
char *decode_image(const char *image_path);
int archive_images(int client_id);

#endif // ENCODE_DECODE_IMAGE_H
