#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>
#include "encode_decode_image.h"
#include <errno.h>

#define BUF_SIZE 1024
#define MAX_CLIENTS 100
#define ADDR "127.0.0.1"

// Structura pentru client
typedef struct {
    int socket;
    int is_admin;
    int id;
    pthread_t thread;
} client_t;

client_t *clients[MAX_CLIENTS]; // Lista de clienti
int client_count = 0; // Numarul de clienti conectati
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex pentru protejarea accesului la lista de clienti
volatile int server_uptime = 1;  // Flag pentru timpul de functionare a serverului
int self_pipe[2];  // Pipe pentru gestionarea semnalelor
volatile int admin_connected = 0;  // Flag pentru a verifica daca un admin este deja conectat

// Functii de gestionare a clientilor si semnalelor
void *client_handler(void *arg);
void *admin_handler(void *arg);
void disconnect_client(int client_id);
void disconnect_all_clients();
void signal_handler(int signal);
void add_client(client_t *client);
void remove_client(int client_id);
const char* get_err_msg(int err);



int main(int argc, char *argv[]) {
    int server_fd, new_socket, PORT;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Verificarea argumentelor de linie de comanda
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    PORT = atoi(argv[1]); // Conversia argumentului port la integer

    if (pipe(self_pipe) == -1) {
        perror("pipe failed"); 
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, signal_handler);  // Setarea handler-ului pentru semnalul SIGINT

    // Crearea socket-ului de server
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Setarea socket-ului
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ADDR);
    address.sin_port = htons(PORT);

    // Legarea socket-ului la adresa si port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Ascultarea conexiunilor
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    // Bucla principala a serverului
    while (server_uptime) {
        fd_set readfds; // Declararea unui set de fd
        FD_ZERO(&readfds); // Resetarea setului fd
        FD_SET(server_fd, &readfds); // Adaugarea serverului la set
        FD_SET(self_pipe[0], &readfds); // Adaugarea pipe-ului pentru gestionarea semnalelor

        int max_fd = (server_fd > self_pipe[0]) ? server_fd : self_pipe[0]; // Determinarea file descriptorului maxim
        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL); // Asteptarea activitatii

        if (activity < 0 && errno != EINTR) {
            perror("select error"); // Mesaj de eroare daca select-ul esueaza
            close(server_fd);
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(self_pipe[0], &readfds)) {
            // Semnal primit
            break;
        }

        if (FD_ISSET(server_fd, &readfds)) {
            // Acceptarea unei conexiuni noi
            new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
            if (new_socket < 0) {
                perror("accept failed");
                continue;
            }

            client_t *client = (client_t *)malloc(sizeof(client_t)); // Alocarea memoriei pentru un nou client
            // Initializare stucturii
            client->socket = new_socket;
            client->is_admin = 0;
            client->id = new_socket;

            // Crearea unui thread pentru client
            if (pthread_create(&client->thread, NULL, client_handler, (void*)client) < 0) {
                perror("could not create thread"); 
                free(client);
                close(new_socket);
                continue;
            }

            pthread_mutex_lock(&clients_mutex); // Blocarea mutex-ului inainte de a modifica lista de clienti
            add_client(client);
            pthread_mutex_unlock(&clients_mutex); // Deblocarea mutex-ului
        }
    }

    close(server_fd); // Inchiderea socket-ului de server
    disconnect_all_clients(); // Deconectarea tuturor clientilor
    return 0;
}




// Functia de gestionare a unui client
void *client_handler(void *arg) {
    client_t *client = (client_t *)arg;
    char buffer[BUF_SIZE];
    int read_size;

    // Determinarea tipului de client din prima comanda
    if ((read_size = recv(client->socket, buffer, BUF_SIZE, 0)) > 0) {
        buffer[read_size] = '\0';

        if (strncmp(buffer, "admin", 5) == 0) {
            // Verifica daca un admin este deja conectat
            if (admin_connected) {
                send(client->socket, "Admin already connected", 25, 0);
                close(client->socket);
                pthread_mutex_lock(&clients_mutex); // Blocarea mutex-ului inainte de a modifica lista de clienti
                remove_client(client->id); // Eliminarea clientului din lista
                pthread_mutex_unlock(&clients_mutex); // Deblocarea mutex-ului
                free(client);
                return NULL;
            }

            client->is_admin = 1;
            admin_connected = 1; // Marcheaza ca un admin este conectat
            printf("Admin client connected with ID: %d\n", client->id);
            send(client->socket, "Welcome, admin", 16, 0);
            admin_handler(client);
            return NULL;
        } else {
            printf("Normal client connected with ID: %d\n", client->id);
            send(client->socket, "Welcome, client", 17, 0);
        }
    }

    // Procesarea comenzilor de la client
    while ((read_size = recv(client->socket, buffer, BUF_SIZE, 0)) > 0) {
        buffer[read_size] = '\0';

        printf("Client with ID %d requested command: %s\n", client->id, buffer);
        if (strncmp(buffer, "encode", 6) == 0) {
            // Extrage calea imaginii si mesajul
            char *token = strtok(buffer + 7, " ");
            char *image_path = token;
            token = strtok(NULL, " ");
            char *message = token;

            if (message == NULL) {
                send(client->socket, "Message cannot be empty", 24, 0);
                continue;
            }

            // Apeleaza functia de codificare
            int res = encode_image(image_path, message, client->id);
            if (res == IMAGE_ENCODED_SUCCESFULY) {
                send(client->socket, "Image encoded!", 16, 0);
                continue;
            }
            const char* err_msg = get_err_msg(res);
            send(client->socket, err_msg, strlen(err_msg), 0);

        } else if (strncmp(buffer, "decode", 6) == 0) {
            // Extrage calea imaginii pentru decodare
            char *image_path = buffer + 7;
            char *message = decode_image(image_path);

            // Trimite mesajul decodat clientului
            send(client->socket, message, strlen(message), 0);
            free(message);
        } else if (strncmp(buffer, "exit", 4) == 0) {
            // Apeleaza functia de arhivare a imaginilor
            int res = archive_images(client->id);

            if (res != ZIP_SUCCES) {
                send(client->socket, "Error creating archive", 23, 0);
            } else {
                send(client->socket, "Archive created successfully", 29, 0);
            }
            printf("Client with ID: %d disconnected\n", client->id);
            break;
        } else {
            // Comanda necunoscuta
            send(client->socket, "Unknown command\n", 16, 0);
        }
    }

    if (read_size == 0) {
        printf("Client disconnected\n"); // Mesaj daca clientul a inchis conexiunea
        fflush(stdout);
    } else if (read_size == -1) {
        perror("recv failed"); // Mesaj de eroare daca primirea datelor esueaza
    }

    close(client->socket); // Inchiderea socket-ului clientului
    pthread_mutex_lock(&clients_mutex); // Blocarea mutex-ului inainte de a modifica lista de clienti
    remove_client(client->id); // Eliminarea clientului din lista
    pthread_mutex_unlock(&clients_mutex); // Deblocarea mutex-ului
    free(client); // Eliberarea memoriei alocate pentru client
    return NULL;
}

// Functia de gestionare a unui client admin
void *admin_handler(void *arg) {
    client_t *client = (client_t *)arg;
    char buffer[BUF_SIZE];
    int read_size;

    // Procesarea comenzilor de la clientul admin
    while ((read_size = recv(client->socket, buffer, BUF_SIZE, 0)) > 0) {
        buffer[read_size] = '\0';

        if (strncmp(buffer, "uptime", 6) == 0) {
            server_uptime = 1;
            send(client->socket, "Server uptime extended", 24, 0);
        } else if (strncmp(buffer, "disconnect", 10) == 0) {
            // Extrage ID-ul clientului de deconectat
            int target_client_id = atoi(buffer + 11);
            send(target_client_id, "exit", 5, 0); // Trimite comanda de iesire
            send(client->socket, "Client disconnected", 20, 0);
            disconnect_client(target_client_id); // Deconecteaza clientul
            printf("Client %d removed from admin\n", target_client_id);
        } else if (strncmp(buffer, "exit", strlen("exit")) == 0) {
            send(client->socket, "Exiting...", 10, 0);
        } else {
            send(client->socket, "Unknown command", 16, 0);
        }
    }

    if (read_size == 0) {
        printf("Admin client disconnected\n");
        admin_connected = 0; 
        fflush(stdout);
    } else if (read_size == -1) {
        perror("recv failed"); // Mesaj de eroare daca primirea datelor esueaza
    }

    close(client->socket); // Inchiderea socket-ului clientului admin
    pthread_mutex_lock(&clients_mutex); // Blocarea mutex-ului inainte de a modifica lista de clienti
    remove_client(client->id); // Eliminarea clientului din lista
    pthread_mutex_unlock(&clients_mutex); // Deblocarea mutex-ului
    free(client); // Eliberarea memoriei alocate pentru client
    return NULL;
}

// Functia de deconectare a unui client
void disconnect_client(int client_id) {
    pthread_mutex_lock(&clients_mutex); // Blocarea mutex-ului inainte de a modifica lista de clienti
    for (int i = 0; i < client_count; i++) {
        if (clients[i]->id == client_id) {
            // Trimite comanda de iesire la client
            send(clients[i]->socket, "exit", 5, 0);

            // Anuleaza thread-ul clientului
            pthread_cancel(clients[i]->thread);

            // Asteapta terminarea thread-ului
            pthread_join(clients[i]->thread, NULL);

            // Inchide socket-ul clientului
            close(clients[i]->socket);

            // Elibereaza structura clientului
            free(clients[i]);

            // Elimina clientul din lista
            clients[i] = clients[client_count - 1];
            clients[client_count - 1] = NULL;
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex); // Deblocarea mutex-ului
}

// Functia de deconectare a tuturor clientilor
void disconnect_all_clients() {
    pthread_mutex_lock(&clients_mutex); // Blocarea mutex-ului inainte de a modifica lista de clienti
    for (int i = 0; i < client_count; i++) {
        // Trimite comanda de iesire la client
        send(clients[i]->socket, "exit", 5, 0);

        // Anuleaza thread-ul clientului
        pthread_cancel(clients[i]->thread);

        // Asteapta terminarea thread-ului
        pthread_join(clients[i]->thread, NULL);

        // Inchide socket-ul clientului
        close(clients[i]->socket);

        // Elibereaza structura clientului
        free(clients[i]);

        clients[i] = NULL;
    }
    client_count = 0;
    pthread_mutex_unlock(&clients_mutex); // Deblocarea mutex-ului
}

// Functia de gestionare a semnalelor
void signal_handler(int signal) {
    if (signal == SIGINT) {
        printf("\nSIGINT received, shutting down server...\n");
        server_uptime = 0;
        write(self_pipe[1], "1", 1);  // Scrie in pipe pentru a intrerupe select-ul
    }
}

// Functia de adaugare a unui client
void add_client(client_t *client) {
    clients[client_count++] = client;
}

// Functia de eliminare a unui client
void remove_client(int client_id) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i]->id == client_id) {
            clients[i] = clients[client_count - 1];
            clients[client_count - 1] = NULL;
            client_count--;
            break;
        }
    }
}

// Functia de obtinere a mesajelor de eroare
const char* get_err_msg(int err) {
    if (err == NOT_VALID_BMP) {
        return "The BMP provided is not valid\n";
    } else if (err == MESSAGE_TOO_LONG) {
        return "The Message is too long\n";
    } else if (err == ERROR_CREATING_ENCODED_IMAGE) {
        return "Error creating encoded image\n";
    } else if (err == ERROR_OPENING_FILE) {
        return "Error opening file\n";
    }
    return "Unknown error\n";
}
