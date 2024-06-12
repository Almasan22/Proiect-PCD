import socket
import threading
import os

BUF_SIZE = 1024
keep_running = True  # Flag pentru a indica daca clientul ar trebui sa continue sa ruleze

# Functia pentru thread-ul de receptie a mesajelor de la server
def receive_handler(sock, pipe_w):
    global keep_running
    while keep_running:
        try:
            data = sock.recv(BUF_SIZE).decode()
            if data:
                os.write(pipe_w, data.encode())
                if data.strip() == "exit":
                    print("\nThe server has requested disconnection. Press enter!")
                    keep_running = False
                    break
        except:
            break

def main():
    global keep_running
    if len(os.sys.argv) != 2:
        print(f"Usage: {os.sys.argv[0]} <port>")
        return

    PORT = int(os.sys.argv[1])
    SERVER_ADDRESS = "127.0.0.1"

    try:
        # Creare, conectare
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((SERVER_ADDRESS, PORT))

        # Trimitere prim mesaj la server
        sock.sendall(b"client")
        data = sock.recv(BUF_SIZE)
        print(data.decode())

        # Crearea pipe-ului pentru comunicare intre thread-uri
        pipe_r, pipe_w = os.pipe()

        # Crearea thread-ului pentru receptia mesajelor de la server
        recv_thread = threading.Thread(target=receive_handler, args=(sock, pipe_w))
        recv_thread.start()

        while keep_running:
            command = input("Enter command (encode <path> <message>, decode <path>, exit): ").strip()
            if not command:
                continue

            sock.sendall(command.encode())

            if command == "exit":
                data = os.read(pipe_r, BUF_SIZE).decode()
                print(f"Server response: {data}")
                break

            try:
                data = os.read(pipe_r, BUF_SIZE).decode()
                if data:
                    print(f"Server response: {data}")
                    if data.strip() == "exit":
                        break
            except:
                break

        keep_running = False
        recv_thread.join()
        sock.close()
        os.close(pipe_r)
        os.close(pipe_w)

    except Exception as e:
        print(f"An error occurred: {e}")
        keep_running = False

if __name__ == "__main__":
    main()
