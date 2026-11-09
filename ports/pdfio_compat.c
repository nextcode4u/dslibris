/* SPDX-License-Identifier: MIT */
#include <sys/stat.h>
#include <unistd.h>

/* PDFio probes the final 1023 bytes even for smaller files. Some FAT drivers
 * reject a seek before byte zero without POSIX EINVAL, so clamp this probe
 * before calling the driver. Keep the SDK and upstream PDFio source unchanged.
 */
off_t reader_pdf_lseek(int fd, off_t offset, int whence)
{
    if (whence == SEEK_END && offset < 0) {
        struct stat info;
        if (!fstat(fd, &info) && info.st_size >= 0 && offset < -info.st_size)
            return lseek(fd, 0, SEEK_SET);
    }
    return lseek(fd, offset, whence);
}
