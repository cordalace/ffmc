#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <fuse.h>
#include <curl/curl.h>

static const char *video_path = "/big-buck-bunny.avi";
static const char *big_buck_bunny_url = "https://velling.ru/media/big-buck-bunny.avi";
static const long big_buck_bunny_len = 332243668L;

struct userdata_t {
    size_t len;
    char* data;
};

static int ffmc_getattr(const char *path, struct stat *stbuf)
{
    int res = 0;

    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if (strcmp(path, video_path) == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = big_buck_bunny_len;
    } else
        res = -ENOENT;

    return res;
}

static int ffmc_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, video_path + 1, NULL, 0);

    return 0;
}

static int ffmc_open(const char *path, struct fuse_file_info *fi)
{
    if (strcmp(path, video_path) != 0)
        return -ENOENT;

    if ((fi->flags & 3) != O_RDONLY)
        return -EACCES;

    return 0;
}

size_t write_callback(void *ptr, size_t size, size_t nmemb,
                      struct userdata_t *s)
{
  size_t new_len = s->len + size * nmemb;
  memcpy(s->data + s->len, ptr, size * nmemb);
  // s->data[new_len] = '\0';
  s->len = new_len;

  return size * nmemb;
}

static int ffmc_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
    char range_str[27];  // 1 TiB (13 chars) + "-" (1 char) + 1 TiB (13 chars)
    CURL *curl;
    struct userdata_t curl_data;
    (void) fi;

    if(strcmp(path, video_path) != 0)
        return -ENOENT;

    if (offset < big_buck_bunny_len) {
        if (offset + size > big_buck_bunny_len)
            size = big_buck_bunny_len - offset;
        curl_data.data = buf;
        curl_data.len = 0;
        sprintf(range_str, "%zu-%zu", offset, offset + size);
        curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_URL, big_buck_bunny_url);
        curl_easy_setopt(curl, CURLOPT_RANGE, range_str);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &curl_data);
        curl_easy_perform(curl); /* ignores error */
        curl_easy_cleanup(curl);
    } else
        size = 0;

    return size;
}

static struct fuse_operations ffmc_oper = {
    .getattr = ffmc_getattr,
    .readdir = ffmc_readdir,
    .open = ffmc_open,
    .read = ffmc_read,
};

int main(int argc, char *argv[])
{
    curl_global_init(CURL_GLOBAL_ALL);
    return fuse_main(argc, argv, &ffmc_oper, NULL);
}
