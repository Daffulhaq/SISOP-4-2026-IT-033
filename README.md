# SISOP-4-2026-IT-033

Nama: Daffa Ulhaq Fadhlurrahman  
NRP: 5027251033

## Soal 1
### Permulaan
Soal 1 mengharuskan praktikan untuk mengunduh sebuah file `zip` yang diberikan dan meng-copy-nya ke direktori `soal_1`. Kemudian, sebuah direktori kosong `mnt` dibuat untuk menjadi mount directory untuk file `fuse` yang akan dibuat dan dijalankan.

<img width="1000" height="100" alt="Screenshot 2026-05-17 153136" src="https://github.com/user-attachments/assets/b56f6112-4fca-44d6-8bb2-b22e5e1a50c5" />

### File kenz_rescue.c
File ini berisi program fuse yang akan digunakan untuk melakukan beberapa operasi.
#### - dir_path 
```c
static char dir_path[1024];
```
Variabel ini digunakan untuk menyimpan path dari source directory yang dipakai oleh FUSE. Variabel tersebut berupa array karakter (string) dengan kapasitas maksimal 1024 karakter.
#### - Fungsi get_fullpath
```c
static void get_fullpath(char fpath[1024], const char *path)
{
    sprintf(fpath, "%s%s", dir_path, path);
}
```
Fungsi ini digunakan untuk membentuk path asli dari file yang berada pada source directory. Karena FUSE bekerja menggunakan path virtual, maka program perlu mengubah path virtual tersebut menjadi path sebenarnya.
#### - Fungsi xmp_getattr
```c
static int xmp_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    char fpath[1024];

    if (strcmp(path, "/tujuan.txt") == 0)
    {
        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_mode = S_IFREG | 0444; // Read-only
        stbuf->st_nlink = 1;
        stbuf->st_size = 1024;
        return 0;
    }

    get_fullpath(fpath, path);
    int res = lstat(fpath, stbuf);
    if (res == -1)
        return -errno;
    return 0;
}
```
Fungsi ini digunakan untuk mengambil metadata file atau direktori.
```c
if (strcmp(path, "/tujuan.txt") == 0)
```
Program  akan mengecek apakah file yang diminta adalah `tujuan.txt`. Jika benar, maka metadata dibuat secara manual karena file tersebut tidak benar-benar ada di disk (virtual).
```c
stbuf->st_mode = S_IFREG | 0444;
stbuf->st_size = 1024;
```
Program ini akan membuat file bertipe `regular`, bersifat `read-only`, dan berukuran 1024 bytes.
```c
get_fullpath(fpath, path);
    int res = lstat(fpath, stbuf);
    if (res == -1)
        return -errno;
    return 0;
```
Jika bukan `tujuan.txt`, maka program mengambil metadata file asli menggunakan `lstat(fpath, stbuf);` dan mengeluarkan pesan error jika gagal.

#### - Fungsi xmp_readdir
Fungsi ini digunakan untuk membuka dan membaca isi direktori.
```c
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
char fpath[1024];
    get_fullpath(fpath, path);

    DIR *dp;
    struct dirent *de;

    dp = opendir(fpath);
    if (dp == NULL)
        return -errno;

    while ((de = readdir(dp)) != NULL)
    {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0, 0))
            break;
    }
    if (strcmp(path, "/") == 0)
    {
        filler(buf, "tujuan.txt", NULL, 0, 0);
    }

    closedir(dp);
    return 0;
}
```
Dengan `while ((de = readdir(dp)) != NULL)`, program akan membuat loop untuk membaca seluruh file dan folder. Setiap file dimasukkan ke output FUSE menggunakan:
```c
filler(buf, de->d_name, &st, 0, 0)
```
Kemudian, jika direktori yang dibuka adalah root filesystem virtual, maka program menambahkan file virtual `tujuan.txt`:
```c
if (strcmp(path, "/") == 0)
    {
        filler(buf, "tujuan.txt", NULL, 0, 0);
    }

    closedir(dp);
    return 0;
```
Akibatnya, file `tujuan.txt` muncul saat user mengetik `ls`, walaupun file tersebut sebenarnya tidak ada di disk.

#### - Fungsi get_coord_fragment
Fungsi ini digunakan untuk mengambil fragmen koordinat dari file tertentu dan membuat isi dari file virtual `tujuan.txt`.
```c
void get_coord_fragment(const char *filename, char *dest)
{
    char fpath[1024];
    sprintf(fpath, "%s/%s", dir_path, filename);
    FILE *f = fopen(fpath, "r");
    if (f)
    {
        char line[256];
        while (fgets(line, sizeof(line), f))
        {
            if (strncmp(line, "KOORD:", 6) == 0)
            {
                strcat(dest, line + 7);
                dest[strcspn(dest, "\r\n")] = 0;
            }
        }
        fclose(f);
    }
}
```
Program ini akan membuka dan membaca isi file per baris. Jika ditemukan baris yang diawali `KOORD`, maka isi setelah `KOORD:` akan digabungkan ke variabel tujuan `dest`.

#### - Fungsi xmp_read
```c
static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
  if (strcmp(path, "/tujuan.txt") == 0)
    {
        char content[1024] = "Tujuan Mas Amba: ";
        char fragments[512] = "";

        for (int i = 1; i <= 7; i++)
        {
            char fname[16];
            sprintf(fname, "%d.txt", i);
            get_coord_fragment(fname, fragments);
        }

        strcat(content, fragments);
        strcat(content, "\n");

        size_t len = strlen(content);
        if (offset < len)
        {
            if (offset + size > len)
                size = len - offset;
            memcpy(buf, content + offset, size);
        }
        else
            size = 0;

        return size;
    }

    char fpath[1024];
    get_fullpath(fpath, path);
    int fd = open(fpath, O_RDONLY);
    if (fd == -1)
        return -errno;

    int res = pread(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);
    return res;
}
```
Fungsi ini digunakan saat file `tujuan.txt` dibaca dengan menggunakan perintah `cat`. Jika file yang dibaca adalah `tujuan.txt`, program membuat isi file secara otomatis.
Program ini bekerja dengan melakukan loop `for (int i = 1; i <= 7; i++)` untuk membaca file 1.txt sampai 7.txt. Setiap file yang dibaca akan diproses menggunakan `get_coord_fragment(fname, fragments);` dan hasil semua fragmen koordinat digabungkan menjadi satu string.

#### - xmp_oper
```c
static const struct fuse_operations xmp_oper = {
    .getattr = xmp_getattr,
    .readdir = xmp_readdir,
    .read = xmp_read,
};
```
Struktur ini digunakan untuk mendaftarkan `callback` filesystem ke FUSE.

#### - Fungsi main
```c
int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Usage: ./kenz_rescue <source_dir> <mount_dir>\n");
        return 1;
    }
    realpath(argv[1], dir_path);
    argv[1] = argv[2];
    return fuse_main(argc - 1, argv, &xmp_oper, NULL);
}
```
Program akan memastikan user memasukkan `source directory` dan `mount directory`. Kemudian, program mengubah source directory menjadi absolute path `dir_path`:
```c
realpath(argv[1], dir_path);
```
Selanjutnya, program akan menggeser argumen agar FUSE menganggap argumen kedua sebagai mount point.
```c
argv[1] = argv[2];
```
Terakhir, filesystem virtual dijalankan:
```c
fuse_main(argc - 1, argv, &xmp_oper, NULL);
```

### Output
Program akan menyalin seluruh file dan isinya dari direktori `amba_files` ke direktori `mnt`. Tetapi, direktori `mnt` mengandung satu file virtual `tujuan.txt`.

<img width="1002" height="155" alt="Screenshot 2026-05-17 163126" src="https://github.com/user-attachments/assets/bcdf03f4-71c2-48b1-ae62-57c79599c1e2" />

Isi file dengan nama yang sama harus berisi yang sama juga meski terletak di dalam direktori yang berbeda

<img width="1451" height="549" alt="Screenshot 2026-05-17 163103" src="https://github.com/user-attachments/assets/6f46bdd5-8b95-4388-a2cb-22c52a0a2f37" />

File virtual `tujuan.txt` akan menyimpan koordinat tujuan:

<img width="1056" height="50" alt="Screenshot 2026-05-17 163137" src="https://github.com/user-attachments/assets/472149f6-2b9a-4b62-8e4e-62a6ee43a5a3" />

### Kendala
Tidak ada.

## Soal 2
### Permulaan
Praktikan diharuskan untuk mengunduh 2 file yang diberikan dari soal.

<img width="773" height="71" alt="Screenshot 2026-05-17 230809" src="https://github.com/user-attachments/assets/7c8ed5a6-d6a7-4e40-8aba-e9251a03659a" />

### File fuse.c
File diawali oleh baris kode:
```c
static const char *storage_path = "/home/astro_cancerous/SISOP-4-2026-IT-033/soal_2/encrypted_storage";
```
untuk menentukan jalur folder fisik di dalam harddisk yang digunakan untuk menyimpan data yang sebenarnya.

#### 1. Fungsi xor_cipher
```c
void xor_cipher(char *data, size_t n)
{
    for (size_t i = 0; i < n; i++)
        data[i] ^= 0x76;
}
```
Fungsi ini melakukan enkripsi dan dekripsi menggunakan operasi bitwise XOR terhadap setiap byte data dengan key `0x76`. Karena sifat XOR yang reversible, fungsi yang sama digunakan untuk menyembunyikan data saat `write` dan mengembalikan data asli saat `read`.

#### 2. Fungsi get_target_path
```c
static void get_target_path(const char *path, char *fpath)
{
    if (strcmp(path, "/") == 0)
    {
        sprintf(fpath, "%s", storage_path);
        return;
    }

    char check_path[1024];
    sprintf(check_path, "%s%s", storage_path, path);

    struct stat st;
    if (stat(check_path, &st) == 0 && S_ISDIR(st.st_mode))
    {
        strcpy(fpath, check_path);
        return;
    }

    sprintf(fpath, "%s%s.enc", storage_path, path);
}
```
Fungsi ini dibuat untuk menerjemahkan path dari mount point FUSE ke jalur folder penyimpanan asli di sistem `(storage_path)`. Jika target berupa direktori, path tetap normal. Jika target berupa file, fungsi ini otomatis menambahkan ekstensi `.enc` di ujung path.

#### 3. Fungsi x_getattr
```c
static int x_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    char fpath[1024];
    get_target_path(path, fpath);
    int res = lstat(fpath, stbuf);
    if (res == -1)
        return -errno;
    return 0;
}
```
Fungsi ini dibuat untuk mengambil atribut atau metadata sebuah file atau direktori menggunakan fungsi `lstat`. 

#### 4. Fungsi x_readdir
```c
static int x_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
    char fpath[1024];
    sprintf(fpath, "%s%s", storage_path, path);
    DIR *dp = opendir(fpath);
    if (dp == NULL)
        return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL)
    {
        char name[256];
        strcpy(name, de->d_name);
        char *ext = strstr(name, ".enc");
        if (ext != NULL && strlen(ext) == 4)
            *ext = '\0';

        if (filler(buf, name, NULL, 0, 0))
            break;
    }
    closedir(dp);
    return 0;
}
```
Fungsi ini akan menampilkan isi dari sebuah direktori. Fungsi ini membuka direktori asli, membaca isinya, dan menyembunyikan ekstensi `.enc` menggunakan fungsi manipulasi `string (strstr dan \0)` sebelum ditampilkan di mount point.

#### 5. Fungsi x_mkdir
```c
static int x_mkdir(const char *path, mode_t mode)
{
    char fpath[1024];
    sprintf(fpath, "%s%s", storage_path, path);
    int res = mkdir(fpath, mode);
    if (res == -1)
        return -errno;
    return 0;
}
```
Fungsi ini dibuat untuk membuat sebuah direktori baru di dalam `storage_path`.

#### 6. Fungsi x_rmdir
```c
static int x_rmdir(const char *path)
{
    char fpath[1024];
    sprintf(fpath, "%s%s", storage_path, path);
    int res = rmdir(fpath);
    if (res == -1)
        return -errno;
    return 0;
}
```
Fungsi ini dibua untuk menghapus sebuah direktori kosong berdasarkan path yang diberikan.

#### 7. Fungsi x_create
```c
static int x_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    char fpath[1024];
    sprintf(fpath, "%s%s.enc", storage_path, path);
    int res = open(fpath, fi->flags, mode);
    if (res == -1)
        return -errno;
    fi->fh = res;
    return 0;
}
```
Fungsi ini akan membuat sebuah file baru. Di sini, sistem otomatis menambahkan ekstensi 1.enc1 pada nama file asli dan menyimpan file descriptor-nya ke dalam variabel `fi->fh`.

#### 8. Fungsi x_open
```c
static int x_open(const char *path, struct fuse_file_info *fi)
{
    char fpath[1024];
    get_target_path(path, fpath);
    int res = open(fpath, fi->flags);
    if (res == -1)
        return -errno;
    fi->fh = res;
    return 0;
}
```
Fungsi ini digunakan untuk membuka file yang ada di sistem. Fungsi ini mencari letak file asli lewat `get_target_path` dan menyimpan file descriptor hasil modifikasi ke `fi->fh` agar bisa dipakai oleh fungsi read/write.

#### 9. Fungsi x_read
```c
static int x_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int res = pread(fi->fh, buf, size, offset);
    if (res > 0)
        xor_cipher(buf, res);
    return res;
}
```
Fungsi ini digunakan untuk membaca isi file menggunakan `pread`. Karena file asli di storage terenkripsi, isi file yang dibaca akan didekripsi terlebih dahulu menggunakan fungsi `xor_cipher`.

#### 10. Fungsi x_write
```c
static int x_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    char *temp = malloc(size);
    memcpy(temp, buf, size);
    xor_cipher(temp, size);
    int res = pwrite(fi->fh, temp, size, offset);
    free(temp);
    return res;
}
```
Fungsi ini digunakan untuk menulis data ke dalam file menggunakan `pwrite`. Sebelum data ditulis ke storage asli, data dialokasikan ke memori sementara `(temp)`, dienkripsi dengan `xor_cipher`, baru kemudian ditulis.

#### 11. Fungsi x_truncate
```c
static int x_truncate(const char *path, off_t size, struct fuse_file_info *fi)
{
    char fpath[1024];
    get_target_path(path, fpath);
    int res = truncate(fpath, size);
    if (res == -1)
        return -errno;
    return 0;
}
```
Fungsi ini akan mengubah ukuran sebuah file ke ukuran 1024 bytes.

#### 12. Fungsi x_unlink
```c
static int x_unlink(const char *path)
{
    char fpath[1024];
    get_target_path(path, fpath);
    int res = unlink(fpath);
    if (res == -1)
        return -errno;
    return 0;
}
```
Fungsi ini digunakan untuk menghapus sebuah file dari sistem dengan memanggil fungsi `unlink` pada file `.enc` yang bersangkutan.

#### 13. Fungsi x_access
```c
static int x_access(const char *path, int mask)
{
    char fpath[1024];
    get_target_path(path, fpath);

    int res = access(fpath, mask);
    if (res == -1)
        return -errno;
    return 0;
}
```
Fungsi ini akan memeriksa hak akses user terhadap suatu file atau direktori.

#### 14. Fungsi x_utimens
```c
static int x_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi)
{
    char fpath[1024];
    get_target_path(path, fpath);
    int res = utimensat(AT_FDCWD, fpath, tv, AT_SYMLINK_NOFOLLOW);
    if (res == -1)
        return -errno;
    return 0;
}
```
Fungsi ini digunakan untuk mengubah `timestamp` akses terakhir dan modifikasi terakhir dari sebuah file menggunakan fungsi `utimensat`.
Semua fungsi diatas kemudian akan dipanggil lagi dengan `struct xmp_oper`:
```c
static const struct fuse_operations x_oper = {
    .getattr = x_getattr,
    .readdir = x_readdir,
    .mkdir = x_mkdir,
    .rmdir = x_rmdir,
    .create = x_create,
    .open = x_open,
    .read = x_read,
    .write = x_write,
    .truncate = x_truncate,
    .unlink = x_unlink,
    .access = x_access,
    .utimens = x_utimens,
};
```

#### 15. Fungsi main
```c
int main(int argc, char *argv[])
{
    struct stat st = {0};
    if (stat(storage_path, &st) == -1)
    {
        mkdir(storage_path, 0777);
    }

    return fuse_main(argc, argv, &x_oper, NULL);
}
```
Fungsi ini merupakan fungsi utama program. Pertama, program memeriksa apakah direktori `storage_path` sudah ada menggunakan `stat`. Jika belum, folder tersebut akan dibuat otomatis menggunakan `mkdir` dengan izin penuh `(0777)`. Setelah itu, program menyerahkan kontrol sistem ke fungsi `fuse_main` untuk menjalankan virtual file system dengan operasi-operasi yang sudah didefinisikan di dalam struct `x_oper`.

### File client.c
File ini mengimplementasikan program TCP Client berbasis socket CLI (Command Line Interface). Tugasnya adalah menghubungkan pengguna ke DB Server yang berjalan di port 9000.
```c
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
        message[strcspn(message, "\n")] = 0; 

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
```
Program ini menginisialisasi socket jaringan lalu melakukan koneksi ke alamat lokal (127.0.0.1) pada port 9000. Setelah berhasil terhubung, program masuk ke dalam looping untuk membaca input perintah dari terminal user, mengirimkannya ke server, mendengarkan balasan server, dan mencetak hasilnya kembali ke layar secara terus-menerus hingga pengguna mengetik perintah `"EXIT"` untuk menutup koneksi.

### File Dockerfile
File ini berfungsi untuk membangun sebuah `image container` berbasis Ubuntu.
```c
FROM ubuntu:latest

WORKDIR /app

COPY server /app/server
RUN chmod +x /app/server

EXPOSE 9000

CMD ["./server"]
```
Di dalam prosesnya, Dockerfile ini membuat direktori bernama `/app`, menyalin file binary bernama `server` ke dalamnya, dan mengubah izin akses file tersebut agar dapat dieksekusi. Terakhir, Dockerfile ini membuka port jaringan 9000 dan mengubah `container` agar otomatis menjalankan `./server` sebagai proses utamanya saat pertama kali dinyalakan.

### Output
#### Fuse
Dijalankan dengan FUSE. Tes dilakukan dengan mengikuti contoh dari pembuat soal

<img width="1919" height="638" alt="Screenshot 2026-05-17 225343" src="https://github.com/user-attachments/assets/600fb5a8-6486-4111-8099-f7d53214868d" />

Kemudian, file `notes.csv.enc` dimasukkan sebagai tanda keberhasilan.

<img width="1466" height="488" alt="Screenshot 2026-05-17 230351" src="https://github.com/user-attachments/assets/00f32580-9292-431c-98ba-92947e066536" />

#### Containerization
Dijalankan dengan FUSE dan Docker.

<img width="1450" height="574" alt="Screenshot 2026-05-17 222223" src="https://github.com/user-attachments/assets/3dab02d5-2cf3-47fa-bf8e-d5cd65eaa5f0" />

<img width="1466" height="129" alt="Screenshot 2026-05-17 230520" src="https://github.com/user-attachments/assets/0e2a228e-e343-4652-8af8-824a3475b5de" />

#### Integration
Dilakukan dengan menjalankan FUSE, Docker, dan client.

<img width="1451" height="225" alt="Screenshot 2026-05-17 230538" src="https://github.com/user-attachments/assets/7a4f48fa-02dd-418d-b584-d6808f67897d" />

<img width="1051" height="637" alt="Screenshot 2026-05-17 223311" src="https://github.com/user-attachments/assets/ec221793-9c5f-4143-8dd8-5e950e406403" />

<img width="1066" height="602" alt="Screenshot 2026-05-17 223319" src="https://github.com/user-attachments/assets/8f4736ac-b792-4bef-9ddc-d287c7f61d42" />

<img width="1106" height="298" alt="Screenshot 2026-05-17 223409" src="https://github.com/user-attachments/assets/34753f8c-1e55-49fb-b7ea-ffef3dcb3800" />

<img width="1119" height="307" alt="Screenshot 2026-05-17 223429" src="https://github.com/user-attachments/assets/d4ce7ea6-1b3f-438e-972f-b5e739364e39" />

Notes : Testing dalam membuat `a.txt` lupa untuk di screenshot.

### Kendala
Saat `./server` dijalankan, ternyata database apapun yang dibuat tidak tersimpan.

<img width="1294" height="156" alt="Screenshot 2026-05-17 224216" src="https://github.com/user-attachments/assets/5bd60020-dcf8-4ec8-b049-03c36e63d3f6" />

<img width="952" height="776" alt="Screenshot 2026-05-17 224709" src="https://github.com/user-attachments/assets/619ac16f-c37c-4be9-a252-139228d2c6b2" />

Praktikan lupa untuk menjalankan `server` ini pada waktu pengerjaan sehingga tidak sempat untuk diperbaiki.
