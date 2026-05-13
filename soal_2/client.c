#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int sock;
    struct sockaddr_in server;
    char message[1024], server_reply[4096];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        printf("Gagal membuat socket\n");
        return 1;
    }

    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons(9000);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Koneksi gagal");
        return 1;
    }

    printf("Connected to DB Server on port 9000\n");
    printf("Type HELP for available commands\n");
    printf("Type EXIT to quit\n");

    while(1) {
        printf("\ndb > ");
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\n")] = 0; // Hapus karakter enter (newline)

        if (strcmp(message, "EXIT") == 0) break;
        if (strlen(message) == 0) continue;

        if (send(sock, message, strlen(message), 0) < 0) {
            printf("Gagal mengirim data\n");
            break;
        }

        memset(server_reply, 0, sizeof(server_reply));
        if (recv(sock, server_reply, sizeof(server_reply), 0) < 0) {
            printf("Gagal menerima balasan\n");
            break;
        }
        
        printf("%s\n", server_reply);
    }

    close(sock);
    return 0;
}
