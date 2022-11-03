#include "extusb_devoptab.h"

#ifdef __cplusplus
extern "C" {
#endif

int __extusb_fs_mkdir(struct _reent *r,
                      const char *path,
                      int mode) {
    char *fixedPath;

    if (!path) {
        r->_errno = EINVAL;
        return -1;
    }

    fixedPath = __extusb_fs_fixpath(r, path);
    if (!fixedPath) {
        return -1;
    }

    // TODO: Use mode to set directory attributes.
    FRESULT fr = f_mkdir(fixedPath);
    free(fixedPath);
    if (fr != FR_OK) {
        r->_errno = __extusb_fs_translate_error(fr);
        return -1;
    }

    return 0;
}

#ifdef __cplusplus
}
#endif