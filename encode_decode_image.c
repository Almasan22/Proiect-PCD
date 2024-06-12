#include "encode_decode_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <zip.h>

int encode_image(const char *image_path, const char *message, int client_id) {
    // Deschide fisierul imagine pentru citire in mod binar
    FILE *image_file = fopen(image_path, "rb");
    if (image_file == NULL) {
        return ERROR_OPENING_FILE;
    }

    // Definirea anteturilor BMP
    BITMAPFILEHEADER file_header;
    BITMAPINFOHEADER info_header;

    // Citirea anteturilor BMP
    fread(&file_header, sizeof(BITMAPFILEHEADER), 1, image_file); // Citire antet fisier
    fread(&info_header, sizeof(BITMAPINFOHEADER), 1, image_file); // Citire antet informatii

    // Validarea fisierului BMP verificand campul bfType
    if (file_header.bfType != 0x4D42) {
        fclose(image_file);
        return NOT_VALID_BMP;
    }

    // Calcularea dimensiunii imaginii in octeti
    int image_size = info_header.biWidth * info_header.biHeight * (info_header.biBitCount / 8);

    // Alocarea memoriei pentru datele imaginii si citirea acestora din fisier
    unsigned char *image_data = (unsigned char *)malloc(image_size);
    fseek(image_file, file_header.bfOffBits, SEEK_SET); // Muta cursorul la inceputul datelor imaginii
    fread(image_data, 1, image_size, image_file); // Citire date imagine
    fclose(image_file);

    // Lungimea mesajului
    int message_len = strlen(message);

    // Verificarea daca mesajul este prea lung pentru a fi ascuns in imagine
    if ((message_len + 4) * 8 > image_size) {
        free(image_data);
        return MESSAGE_TOO_LONG;
    }

    // Ascunderea lungimii mesajului in primii 32 de biti
    for (int bit = 0; bit < 32; bit++) {
        int image_index = bit;
        image_data[image_index] = (image_data[image_index] & 0xFE) | ((message_len >> bit) & 0x01);
    }

    // Ascunderea mesajului in imagine
    for (int i = 0; i < message_len; i++) {
        for (int bit = 0; bit < 8; bit++) {
            int image_index = 32 + i * 8 + bit;
            image_data[image_index] = (image_data[image_index] & 0xFE) | ((message[i] >> bit) & 0x01);
        }
    }

    // Construirea numelui noului fisier
    char client_folder[512];
    snprintf(client_folder, sizeof(client_folder), "decoded_images_client_%d", client_id);

    // Verificarea daca folderul specific clientului exista, crearea acestuia daca nu exista
    struct stat st = {0};
    if (stat(client_folder, &st) == -1) {
        mkdir(client_folder, 0700); // Creare director cu permisiuni 0700
    }

    // Construirea caii noului fisier imagine
    const char *filename = strrchr(image_path, '/');
    filename = filename ? filename + 1 : image_path;

    size_t new_image_path_len = strlen(client_folder) + strlen(filename) + 14;
    char *new_image_path = (char *)malloc(new_image_path_len);

    if (new_image_path == NULL) {
        free(image_data);
        return ERROR_CREATING_ENCODED_IMAGE;
    }

    snprintf(new_image_path, new_image_path_len, "%s/%s_encoded.bmp", client_folder, filename);

    // Salvarea imaginii codificate
    FILE *encoded_image_file = fopen(new_image_path, "wb");
    if (encoded_image_file == NULL) {
        free(image_data);
        free(new_image_path);
        return ERROR_CREATING_ENCODED_IMAGE;
    }

    // Scrierea anteturilor BMP
    fwrite(&file_header, sizeof(BITMAPFILEHEADER), 1, encoded_image_file); // Scriere antet fisier
    fwrite(&info_header, sizeof(BITMAPINFOHEADER), 1, encoded_image_file); // Scriere antet informatii

    // Scrierea datelor modificate ale imaginii
    fseek(encoded_image_file, file_header.bfOffBits, SEEK_SET); // Muta cursorul la inceputul datelor imaginii
    fwrite(image_data, 1, image_size, encoded_image_file); // Scriere date imagine

    // Curatarea si inchiderea fisierului
    free(image_data);
    fclose(encoded_image_file);
    printf("Mesajul a fost codificat cu succes. Salvat ca %s\n", new_image_path);
    free(new_image_path); // Eliberare memorie alocata pentru calea fisierului nou

    return IMAGE_ENCODED_SUCCESFULY;
}

char *decode_image(const char *image_path) {
    // Deschide fisierul imagine pentru citire in mod binar
    FILE *image_file = fopen(image_path, "rb");
    char *err_msg = malloc(ERR_LEN);
    if (image_file == NULL) {
        memcpy(err_msg, "Eroare la deschidere fisier", 28);
        return err_msg;
    }

    BITMAPFILEHEADER file_header;
    BITMAPINFOHEADER info_header;

    // Citirea anteturilor BMP
    fread(&file_header, sizeof(BITMAPFILEHEADER), 1, image_file); // Citire antet fiwier
    fread(&info_header, sizeof(BITMAPINFOHEADER), 1, image_file); // Citire antet informatii

    // Verificarea validitatii fisierului BMP
    if (file_header.bfType != 0x4D42) {
        fclose(image_file);
        memcpy(err_msg, "Fișier BMP invalid", 18);
        return err_msg;
    }

    // Calcularea dimensiunii imaginii in octeti
    int image_size = info_header.biWidth * info_header.biHeight * (info_header.biBitCount / 8);

    // Citirea datelor imaginii
    unsigned char *image_data = (unsigned char *)malloc(image_size);
    fseek(image_file, file_header.bfOffBits, SEEK_SET); // Muta cursorul la inceputul datelor imaginii
    fread(image_data, 1, image_size, image_file); // Citire date imagine

    // Decodarea lungimii mesajului din primii 32 de biti
    int message_len = 0;
    for (int bit = 0; bit < 32; bit++) {
        message_len |= (image_data[bit] & 0x01) << bit;
    }

    // Decodarea mesajului din imagine
    char *message = (char *)malloc(message_len + 1);
    for (int i = 0; i < message_len; i++) {
        message[i] = 0;
        for (int bit = 0; bit < 8; bit++) {
            int image_index = 32 + i * 8 + bit;
            message[i] |= (image_data[image_index] & 0x01) << bit;
        }
    }
    message[message_len] = '\0'; // Terminator null pentru mesaj

    // Curatarea si inchiderea fisierului
    free(image_data);
    fclose(image_file);
    return message; // Returneaza mesajul decodat
}


int archive_images(int client_id) {
    char zip_filename[256];
    char dir_path[256];

    // Construieste calea catre folderul specific clientului si numele arhivei zip
    snprintf(dir_path, sizeof(dir_path), "decoded_images_client_%d", client_id);
    snprintf(zip_filename, sizeof(zip_filename), "decodedimages_%d.zip", client_id);

    // Creeaza arhiva zip
    int error = 0;
    zip_t *zip = zip_open(zip_filename, ZIP_CREATE | ZIP_TRUNCATE, &error);
    if (!zip) {
        fprintf(stderr, "Eroare la crearea fișierului zip: %s\n", zip_strerror(zip));
        return ERROR_CREATING_ZIP_FILE;
    }

    // Deschide directorul specific clientului
    DIR *dir = opendir(dir_path);
    if (!dir) {
        perror("opendir");
        zip_close(zip);
        return ERROR_OPENING_FILE;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) { // Verifica daca este un fisier nromal
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "decoded_images_client_%d/%s", client_id, entry->d_name);

            // Deschide fisierul pentru citire
            FILE *file = fopen(filepath, "rb");
            if (!file) {
                perror("fopen");
                continue; // Continua daca nu poate deschide fisierul
            }

            fseek(file, 0, SEEK_END);
            long filesize = ftell(file); // Obtine dimensiunea fisierului
            fseek(file, 0, SEEK_SET);

            // Aloca memorie pentru continutul fisierului si citeste datele
            void *buffer = malloc(filesize);
            if (!buffer) {
                perror("malloc");
                fclose(file);
                continue; // Continua daca alocarea memoriei esueaza
            }

            fread(buffer, 1, filesize, file); // Citeste datele fisierului
            fclose(file); // Inchide fisierul

            // Creeaza sursa zip din bufferul citit
            zip_source_t *source = zip_source_buffer(zip, buffer, filesize, 0);
            if (!source) {
                fprintf(stderr, "Eroare la crearea sursei zip pentru %s: %s\n", filepath, zip_strerror(zip));
                free(buffer);
                continue; // Continua daca nu poate crea sursa zip
            }

            // Adauga fisierul in arhiva zip
            if (zip_file_add(zip, entry->d_name, source, ZIP_FL_OVERWRITE) < 0) {
                fprintf(stderr, "Eroare la adăugarea fișierului %s în zip: %s\n", filepath, zip_strerror(zip));
                zip_source_free(source);
                free(buffer);
            }
        }
    }

    closedir(dir); // Inchide directorul
    if (zip_close(zip) < 0) {
        fprintf(stderr, "Eroare la închiderea fișierului zip: %s\n", zip_strerror(zip));
    } else {
        printf("Arhiva a fost creată cu succes: %s\n", zip_filename);
    }

    return ZIP_SUCCES;
}
