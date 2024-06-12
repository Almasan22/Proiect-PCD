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

// Functia pentru trimiterea unei comenzi la server
void send_command(int sock, const char *command);

int main(int argc, char *argv[]) {
    int sock, PORT;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];

    // Verificarea numarului de argumente
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    // Conversia argumentului de port la integer
    PORT = atoi(argv[1]);

    // Crearea socket-ului
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error"); 
        return -1;
    }

    // Configurarea adresei serverului
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // Conversia adresei IP din format text in format binar
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return -1;
    }

    // Conectarea la server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed"); // Afiseaza mesaj de eroare daca conexiunea a esuat
        return -1;
    }

    // Trimiterea unui mesaj de tip "admin" la server
    send(sock, "admin", strlen("admin"), 0);
    // Citirea raspunsului de la server
    read(sock, buffer, BUF_SIZE);
    // Afisarea raspunsului serverului
    printf("%s\n", buffer);

    if (strcmp(buffer, "Admin already connected") == 0) {
        return 0;
    }
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
        perror("Could not create receive thread");
        return 1;
    }

    // Bucla principala pentru trimiterea comenzilor catre server
    while (keep_running) {
        // Solicitarea unei comenzi de la utilizator
        printf("Enter command (uptime, disconnect <ID_CLIENT>, exit): ");
        fgets(buffer, BUF_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0;  // Elimina caracterul de newline

        if (strncmp(buffer, "exit", 4) == 0) {
            // Trimite comanda de iesire la server
            send_command(sock, "exit");
            read(pipefd[0], buffer, BUF_SIZE);
            printf("Server response: %s\n", buffer);
            memset(buffer, 0, BUF_SIZE);
            break;
        } else if (strlen(buffer) != 0){
            // Trimite comanda la server
            send_command(sock, buffer);
            read(pipefd[0], buffer, BUF_SIZE);
            printf("Server response: %s\n", buffer);
            memset(buffer, 0, BUF_SIZE);
            continue;
        }

        // Verifica daca exista mesaje in pipe de la thread-ul de receptie
        int bytes_read = read(pipefd[0], buffer, BUF_SIZE);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0'; // Asigura ca buffer este terminat corect
            printf("Server response: %s\n", buffer);
            if (strncmp(buffer, "exit", 4) == 0) {
                break;
            }
        }

        // Golirea bufferului
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
        } else if (read_size == 0) {
            // Serverul s-a deconectat
            printf("Server disconnected.\n");
            keep_running = 0; // Setarea flag-ului pentru a opri bucla principala
            break;
        } else {
            // Eroare la primirea mesajului
            perror("recv failed");
            keep_running = 0; // Setarea flag-ului pentru a opri bucla principala
            break;
        }
        // Golirea bufferului
        memset(recv_buffer, 0, BUF_SIZE);
    }

    return NULL;
}

// Functia pentru trimiterea unei comenzi la server
void send_command(int sock, const char *command) {
    send(sock, command, strlen(command), 0);
}
