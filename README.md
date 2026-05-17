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
