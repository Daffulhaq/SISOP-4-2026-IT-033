#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>

// GANTI INI dengan absolute path ke folder encrypted_storage kamu!
static const char *storage_path = "/home/astro_cancerous/SISOP-4-2026-IT-033/soal_2/encrypted_storage";

// Fungsi XOR Cipher (Key: 0x76)
void xor_cipher(char *data, size_t n)
{
    for (size_t i = 0; i < n; i++)
        data[i] ^= 0x76;
}

// Cek apakah path adalah folder atau file, lalu tentukan path aslinya di storage
static void get_target_path(const char *path, char *fpath)
{
    // 1. Cek kondisi root "/" terlebih dahulu sebelum melakukan stat
    if (strcmp(path, "/") == 0)
    {
        sprintf(fpath, "%s", storage_path);
        return;
    }

    // 2. Buat path sementara untuk memeriksa apakah targetnya adalah direktori murni
    char check_path[1024];
    sprintf(check_path, "%s%s", storage_path, path);

    struct stat st;
    if (stat(check_path, &st) == 0 && S_ISDIR(st.st_mode))
    {
        // Jika terbukti direktori asli, gunakan path tanpa .enc
        strcpy(fpath, check_path);
        return;
    }

    // 3. Jika bukan direktori, asumsikan itu file dan WAJIB tambahkan .enc di ujungnya
    sprintf(fpath, "%s%s.enc", storage_path, path);
}

// 1. GETATTR
static int x_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    char fpath[1024];
    get_target_path(path, fpath);
    int res = lstat(fpath, stbuf);
    if (res == -1)
        return -errno;
    return 0;
}

// 2. READDIR
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

        // Hilangkan .enc agar di fuse_mount terlihat normal
        char *ext = strstr(name, ".enc");
        if (ext != NULL && strlen(ext) == 4)
            *ext = '\0';

        if (filler(buf, name, NULL, 0, 0))
            break;
    }
    closedir(dp);
    return 0;
}

// 3. MKDIR (Folder tidak di-XOR dan tidak pakai .enc)
static int x_mkdir(const char *path, mode_t mode)
{
    char fpath[1024];
    sprintf(fpath, "%s%s", storage_path, path);
    int res = mkdir(fpath, mode);
    if (res == -1)
        return -errno;
    return 0;
}

// 4. RMDIR
static int x_rmdir(const char *path)
{
    char fpath[1024];
    sprintf(fpath, "%s%s", storage_path, path);
    int res = rmdir(fpath);
    if (res == -1)
        return -errno;
    return 0;
}

// 5. CREATE (File baru ditambah .enc)
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

// 6. OPEN
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

// 7. READ (Dekripsi)
static int x_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int res = pread(fi->fh, buf, size, offset);
    if (res > 0)
        xor_cipher(buf, res);
    return res;
}

// 8. WRITE (Enkripsi)
static int x_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    char *temp = malloc(size);
    memcpy(temp, buf, size);
    xor_cipher(temp, size);
    int res = pwrite(fi->fh, temp, size, offset);
    free(temp);
    return res;
}

// 9. TRUNCATE
static int x_truncate(const char *path, off_t size, struct fuse_file_info *fi)
{
    char fpath[1024];
    get_target_path(path, fpath);
    int res = truncate(fpath, size);
    if (res == -1)
        return -errno;
    return 0;
}

// 10. UNLINK (Hapus File)
static int x_unlink(const char *path)
{
    char fpath[1024];
    get_target_path(path, fpath);
    int res = unlink(fpath);
    if (res == -1)
        return -errno;
    return 0;
}

// 11. ACCESS
static int x_access(const char *path, int mask)
{
    char fpath[1024];
    get_target_path(path, fpath);

    int res = access(fpath, mask);
    if (res == -1)
        return -errno;
    return 0;
}

// 12. UTIMENS
static int x_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi)
{
    char fpath[1024];
    get_target_path(path, fpath);
    // Backward compatibility for standard utimensat
    int res = utimensat(AT_FDCWD, fpath, tv, AT_SYMLINK_NOFOLLOW);
    if (res == -1)
        return -errno;
    return 0;
}

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

int main(int argc, char *argv[])
{
    // 1. Pastikan folder encrypted_storage ada sebelum FUSE jalan
    struct stat st = {0};
    if (stat(storage_path, &st) == -1)
    {
        mkdir(storage_path, 0777);
    }

    // 2. Teruskan langsung argv ke fuse_main tanpa diotak-atik manual
    return fuse_main(argc, argv, &x_oper, NULL);
}