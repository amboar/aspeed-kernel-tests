#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define ATTR_LEN 20

ssize_t read_attr(const char *root, const char *path, char *buf, size_t len)
{
    char *cursor;
    char *attr;
    int fd;
    int rc;

    rc = asprintf(&attr, "%s/%s", root, path);
    if (rc == -1) {
        perror("asprintf");
        return rc;
    }

    printf("%s: Reading from %s\n", __func__, attr);

    fd = open(attr, O_RDONLY);
    free(attr);
    if (fd == -1) {
        perror("open");
        return rc;
    }

    cursor = buf;
    rc = 0;
    while ((rc = read(fd, cursor, (buf + len) - cursor)) > 0) {
        cursor += rc;
    }

    if (rc == -1) {
        perror("read");
        return rc;
    }

    buf[len - 1] = '\0';
    *(strchrnul(buf, '\n')) = '\0';

    printf("%s: Read '%s'\n", __func__, buf);

    return cursor - buf;
}

int map_write32(uint8_t *map, size_t offset, uint32_t value)
{
    uint32_t lev = htole32(value);
    uint8_t *plev = (void *)&lev;
    map[offset++] = *plev++;
    map[offset++] = *plev++;
    map[offset++] = *plev++;
    map[offset++] = *plev++;
}

#define streq(...) (!strcmp(__VA_ARGS__))

// USAGE: uio SYSFS [OFFSET BITS VALUE]

int main(int argc, char **argv)
{
    char attr[ATTR_LEN];
    size_t offset;
    char *devname;
    uint8_t *map;
    size_t size;
    char *syspath;
    char *devpath;
    int fd;
    int rc;

    if (argc < 2) {
        printf("Need UIO device name as an argument\n");
        return 1;
    }

    devname = argv[1];
    rc = asprintf(&syspath, "/sys/class/uio/%s", devname);
    if (rc == -1) {
        perror("asprintf");
        return __LINE__;
    }

    rc = read_attr(syspath, "name", attr, sizeof(attr));
    if (rc == -1) {
        return __LINE__;
    }

    if (!streq("scratch", attr)) {
        printf("Unexpected UIO type: %s\n", attr);
        return __LINE__;
    }

    rc = read_attr(syspath, "version", attr, sizeof(attr));
    if (rc == -1) {
        return __LINE__;
    }

    if (!streq("devicetree", attr)) {
        printf("Unexpected UIO version: %s\n", attr);
        return __LINE__;
    }

    rc = read_attr(syspath, "maps/map0/offset", attr, sizeof(attr));
    if (rc == -1) {
        printf("Failed to read map0 offset\n");
        return __LINE__;
    }

    errno = 0;
    offset = strtol(attr, NULL, 0);
    if (errno != 0) {
        perror("strtol");
        return __LINE__;
    }

    rc = read_attr(syspath, "maps/map0/size", attr, sizeof(attr));
    free(syspath);
    if (rc == -1) {
        printf("Failed to read map0 size\n");
        return __LINE__;
    }

    errno = 0;
    size = strtol(attr, NULL, 0);
    if (errno != 0) {
        perror("strtol");
        return __LINE__;
    }

    printf("Map size: 0x%zx\n", size);

    rc = asprintf(&devpath, "/dev/%s", devname);
    if (rc == -1) {
        perror("asprintf");
        return __LINE__;
    }

    printf("Opening device: %s\n", devpath);

    fd = open(devpath, O_RDWR | O_SYNC);
    free(devpath);
    if (fd == -1) {
        perror("open");
        return __LINE__;
    }
    
    map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        return __LINE__;
    }

    if (argc == 5) {
        uint32_t woffset, wbits, wvalue;
        errno = 0;
        woffset = strtol(argv[2], NULL, 0);
        if (errno != 0) {
            perror("strtol");
            return __LINE__;
        }
        wbits = strtol(argv[3], NULL, 0);
        if (errno != 0) {
            perror("strtol");
            return __LINE__;
        }
        wvalue = strtol(argv[3], NULL, 0);
        if (errno != 0) {
            perror("strtol");
            return __LINE__;
        }

        if (wbits != 32) {
            printf("Unsupported write width: %d\n", wbits);
            return __LINE__;
        }

        rc = map_write32(map, woffset, wvalue);
        if (!rc) {
            printf("Failed to write value to map: %d\n", rc);
            return __LINE__;
        }
    }

    return 0;
}
