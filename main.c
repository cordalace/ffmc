#define FUSE_USE_VERSION 26
#define FFMC_MAX_BUFFER_SIZE 1048576

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <fuse.h>
#include <curl/curl.h>
#include <pthread.h>

static const char *video_path = "/big-buck-bunny.avi";
static const char *big_buck_bunny_url = "https://velling.ru/media/big-buck-bunny.avi";
static const long big_buck_bunny_len = 332243668L;
static pthread_mutex_t mutex_buffer;
static char buffer[FFMC_MAX_BUFFER_SIZE];
static size_t buffer_offset = 0;
static size_t buffer_size = 0;

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

void download(size_t start)
{
    struct userdata_t curl_data;
    CURL *curl;
    size_t end;
    char range_str[27];  // 1 TiB (13 chars) + "-" (1 char) + 1 TiB (13 chars)
    if (start + FFMC_MAX_BUFFER_SIZE - 1 > big_buck_bunny_len) {
        end = big_buck_bunny_len;
    }
    else {
        end = start + FFMC_MAX_BUFFER_SIZE - 1;
    }

    curl_data.data = buffer;
    curl_data.len = 0;
    sprintf(range_str, "%zu-%zu", start, end);
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, big_buck_bunny_url);
    curl_easy_setopt(curl, CURLOPT_RANGE, range_str);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &curl_data);
    curl_easy_perform(curl); /* ignores error */
    buffer_offset = start;
    buffer_size = end - start + 1;
    curl_easy_cleanup(curl);
}

void cut_buffer(size_t offset, size_t size, char *buf)
{
    char *copy_start = buffer + offset - buffer_offset;
    size_t move_bytes = buffer_size - offset + buffer_offset - size;
    memcpy(buf, copy_start, size);
    memmove(buffer, copy_start + size, move_bytes);
    buffer_offset = offset + size;
    buffer_size = move_bytes;
}

static int ffmc_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
    (void) fi;
    int end;
    int buffer_end;

    if(strcmp(path, video_path) != 0)
        return -ENOENT;

    if ((offset < big_buck_bunny_len) && (size <= FFMC_MAX_BUFFER_SIZE)) {
        if (offset + size > big_buck_bunny_len)
            size = big_buck_bunny_len - offset;
        end = offset + size;
        pthread_mutex_lock(&mutex_buffer);
        buffer_end = buffer_offset + buffer_size;
        if ((offset < buffer_offset) || (end > buffer_end)) {
            download(offset);
        }
        cut_buffer(offset, size, buf);
        pthread_mutex_unlock(&mutex_buffer);
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
    int ret = 0;
    curl_global_init(CURL_GLOBAL_ALL);
    pthread_mutex_init(&mutex_buffer, NULL);
    ret = fuse_main(argc, argv, &ffmc_oper, NULL);
    pthread_mutex_destroy(&mutex_buffer);
    return ret;
}
