#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

static char dir_path[1024];

// Fungsi untuk mendapatkan path asli dari file di source directory
static void get_fullpath(char fpath[1024], const char *path)
{
    sprintf(fpath, "%s%s", dir_path, path);
}

// Implementasi getattr untuk mendapatkan metadata file
static int xmp_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    char fpath[1024];

    // Logika untuk file virtual tujuan.txt
    if (strcmp(path, "/tujuan.txt") == 0)
    {
        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_mode = S_IFREG | 0444; // Read-only
        stbuf->st_nlink = 1;
        // Ukuran menyesuaikan konten gabungan (kira-kira)
        stbuf->st_size = 1024;
        return 0;
    }

    get_fullpath(fpath, path);
    int res = lstat(fpath, stbuf);
    if (res == -1)
        return -errno;
    return 0;
}

// Implementasi readdir untuk menampilkan daftar file
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi,
                       enum fuse_readdir_flags flags)
{
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

    // Tambahkan file virtual tujuan.txt jika di root
    if (strcmp(path, "/") == 0)
    {
        filler(buf, "tujuan.txt", NULL, 0, 0);
    }

    closedir(dp);
    return 0;
}

// Fungsi pembantu untuk mengambil fragmen KOORD dari file
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
                // Ambil string setelah "KOORD: "
                strcat(dest, line + 7);
                // Hapus newline jika ada
                dest[strcspn(dest, "\r\n")] = 0;
            }
        }
        fclose(f);
    }
}

// Implementasi read untuk membaca isi file
static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{

    // Logika baca khusus untuk tujuan.txt (on-the-fly)
    if (strcmp(path, "/tujuan.txt") == 0)
    {
        char content[1024] = "Tujuan Mas Amba: ";
        char fragments[512] = "";

        // Gabungkan fragmen dari 1.txt sampai 7.txt
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

    // Passthrough untuk file lainnya
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

static const struct fuse_operations xmp_oper = {
    .getattr = xmp_getattr,
    .readdir = xmp_readdir,
    .read = xmp_read,
};

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Usage: ./kenz_rescue <source_dir> <mount_dir>\n");
        return 1;
    }

    // Simpan absolute path dari source directory
    realpath(argv[1], dir_path);

    // Geser argumen agar FUSE menganggap argv[2] sebagai mount point
    argv[1] = argv[2];
    return fuse_main(argc - 1, argv, &xmp_oper, NULL);
}