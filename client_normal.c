#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUF_SIZE 1024

volatile int keep_running = 1; // Flag pentru a indica daca clientul ar trebui sa continue sa ruleze

int pipefd[2]; // Pipe pentru comunicare intre thread-uri

// Functia pentru thread-ul de receptie a mesajelor de la server
void *receive_handler(void *sock_desc);

int main(int argc, char *argv[]) {
    int sock = 0, PORT;
    struct sockaddr_in serv_addr;
    char buffer[BUF_SIZE] = {0};
    char command[BUF_SIZE] = {0};

    // Verificarea numarului de argumente
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    // Conversia argumentului de port la integer
    PORT = atoi(argv[1]);

    // Crearea socket-ului
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\nEroare la crearea socket-ului\n");
        return -1;
    }

    // Configurarea adresei serverului
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Conversia adresei IP din format text in format binar
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nAdresă invalidă / Adresă nesuportată\n");
        return -1;
    }

    // Conectarea la server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConexiunea a eșuat\n");
        return -1;
    }

    // Trimiterea unui mesaj de tip "client" la server
    send(sock, "client", strlen("client"), 0);
    // Citirea raspunsului de la server
    read(sock, buffer, BUF_SIZE);
    // Afisarea raspunsului serverului
    printf("%s\n", buffer);
    // Golirea bufferului
    memset(buffer, 0, BUF_SIZE);

    // Crearea pipe-ului pentru comunicare intre thread-uri
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        return 1;
    }

    // Crearea thread-ului pentru receptia mesajelor de la server
    pthread_t receive_thread;
    if (pthread_create(&receive_thread, NULL, receive_handler, (void *)&sock) < 0) {
        perror("Nu s-a putut crea thread-ul de recepție");
        return 1;
    }

    // Bucla principala pentru trimiterea comenzilor catre server
    while (keep_running) {
        // Solicitarea unei comenzi de la utilizator
        printf("Enter command (encode <path> <message>, decode <path>, exit): ");
        if (fgets(command, BUF_SIZE, stdin) == NULL) {
            break;
        }

        if (!keep_running) {
            break; // Iesire din bucla daca flag-ul este setat la 0
        }

        command[strlen(command) - 1] = '\0'; // Eliminarea newline-ului

        // Trimiterea comenzii la server
        send(sock, command, strlen(command), 0);

        if (strncmp(command, "exit", strlen("exit")) == 0) {
            read(pipefd[0], buffer, BUF_SIZE);
            printf("Server response: %s\n", buffer);
            break;
        }

        // Verifica daca exista mesaje in pipe de la thread-ul de receptie
        int bytes_read = read(pipefd[0], buffer, BUF_SIZE);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0'; // Asigura-te ca bufferul este terminat corect
            printf("Server response: %s\n", buffer);
            if (strncmp(buffer, "exit", 4) == 0) {
                break;
            }
        }

        // Golirea bufferelor
        memset(command, 0, BUF_SIZE);
        memset(buffer, 0, BUF_SIZE);
    }

    // Anularea thread-ului de receptie si asteptarea terminarii acestuia
    pthread_cancel(receive_thread);
    pthread_join(receive_thread, NULL);

    // Inchiderea socket-ului si a pipe-ului
    close(sock);
    close(pipefd[0]);
    close(pipefd[1]);
    return 0;
}

// Functia pentru thread-ul de receptie a mesajelor de la server
void *receive_handler(void *sock_desc) {
    // Extragerea socket-ului description
    int sock = *(int *)sock_desc;
    char recv_buffer[BUF_SIZE];

    while (keep_running) {
        // Citirea mesajului de la server
        int read_size = read(sock, recv_buffer, BUF_SIZE);
        if (read_size > 0) {
            recv_buffer[read_size] = '\0'; // Terminarea bufferului

            // Trimite mesajul in pipe
            write(pipefd[1], recv_buffer, strlen(recv_buffer) + 1);

            if (strncmp(recv_buffer, "exit", 4) == 0) {
                printf("\nThe server has requested disconnection. Press enter!\n");
                keep_running = 0; // Setarea flag-ului pentru a opri bucla principala
                break;
            }
        }
        // Golirea bufferului
        memset(recv_buffer, 0, BUF_SIZE);
    }

    return NULL;
}
