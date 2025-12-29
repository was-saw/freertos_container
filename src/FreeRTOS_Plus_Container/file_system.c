#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "file_system.h"
#include "task.h"

#ifdef configUSE_LITTLEFS
#include "lfs.h"

/* External function from lfs_init.c */
extern int init_littlefs(lfs_t *lfs);

/* Global file system instance */
static FileSystem_t xGlobalFileSystem = {
    .fs_ops = NULL,
    .pvFsContext = NULL,
    .filesystem = configFILESYSTEM_KIND,
    .xInitialized = pdFALSE,
    .xMounted = pdFALSE,
};

static char tmp_path[configMAX_PATH_LEN];

/*-----------------------------------------------------------
 * HELPER FUNCTIONS
 *----------------------------------------------------------*/

/**
 * @brief Get the root path for the current task and build the full path
 * 
 * @param path Original path
 * @param full_path Buffer to store the full path
 * @param max_len Maximum length of the buffer
 * @return Pointer to the full path, or NULL on error
 */
static const char *fs_build_full_path(const char *path, char *full_path, size_t max_len) {
    if (path == NULL || full_path == NULL) {
        return NULL;
    }

    /* Get current task's root path - you need to implement this based on your task structure */
    /* For now, we'll use a placeholder function */
    char root_path[configMAX_PATH_LEN];
    pvTaskGetRootPath(xTaskGetCurrentTaskHandle(), root_path);
    
    /* If path is absolute and root is "/", just use the path as-is */
    if (path[0] == '/' && root_path[0] == '/' && root_path[1] == '\0') {
        return path;
    }
    
    /* Build full path: root_path + path */
    size_t root_len = 0;
    size_t path_len = 0;
    size_t i = 0, j = 0;
    
    /* Calculate root path length */
    while (root_path[root_len] != '\0' && root_len < max_len - 1) {
        root_len++;
    }
    
    /* Calculate input path length */
    while (path[path_len] != '\0' && path_len < max_len - 1) {
        path_len++;
    }
    
    /* Copy root path */
    for (i = 0; i < root_len && i < max_len - 1; i++) {
        full_path[i] = root_path[i];
    }
    
    /* Remove trailing slash from root if present */
    if (i > 0 && full_path[i - 1] == '/' && path[0] == '/') {
        i--;
    }
    
    /* Append the original path */
    for (j = 0; j < path_len && i < max_len - 1; j++, i++) {
        full_path[i] = path[j];
    }
    
    /* Null terminate */
    full_path[i] = '\0';
    
    return full_path;
}

/*-----------------------------------------------------------
 * WRAPPER FUNCTIONS - Forward declarations
 *----------------------------------------------------------*/
#ifndef LFS_READONLY
static int fs_remove_wrapper(const char *path);
static int fs_rename_wrapper(const char *oldpath, const char *newpath);
#endif
static int fs_stat_wrapper(const char *path, struct lfs_info *info);
static lfs_ssize_t fs_getattr_wrapper(const char *path,
                                      uint8_t type, void *buffer, lfs_size_t size);
#ifndef LFS_READONLY
static int fs_setattr_wrapper(const char *path,
                              uint8_t type, const void *buffer, lfs_size_t size);
static int fs_removeattr_wrapper(const char *path, uint8_t type);
#endif
#ifndef LFS_NO_MALLOC
static int fs_file_open_wrapper(lfs_file_t *file,
                                const char *path, int flags);
#endif
static int fs_file_opencfg_wrapper(lfs_file_t *file,
                                   const char *path, int flags,
                                   const struct lfs_file_config *config);
static int fs_file_close_wrapper(lfs_file_t *file);
static int fs_file_sync_wrapper(lfs_file_t *file);
static lfs_ssize_t fs_file_read_wrapper(lfs_file_t *file,
                                        void *buffer, lfs_size_t size);
#ifndef LFS_READONLY
static lfs_ssize_t fs_file_write_wrapper(lfs_file_t *file,
                                         const void *buffer, lfs_size_t size);
#endif
static lfs_soff_t fs_file_seek_wrapper(lfs_file_t *file,
                                       lfs_soff_t off, int whence);
#ifndef LFS_READONLY
static int fs_file_truncate_wrapper(lfs_file_t *file, lfs_off_t size);
#endif
static lfs_soff_t fs_file_tell_wrapper(lfs_file_t *file);
static int fs_file_rewind_wrapper(lfs_file_t *file);
static lfs_soff_t fs_file_size_wrapper(lfs_file_t *file);
#ifndef LFS_READONLY
static int fs_mkdir_wrapper(const char *path);
#endif
static int fs_dir_open_wrapper(lfs_dir_t *dir, const char *path);
static int fs_dir_close_wrapper(lfs_dir_t *dir);
static int fs_dir_read_wrapper(lfs_dir_t *dir, struct lfs_info *info);
static int fs_dir_seek_wrapper(lfs_dir_t *dir, lfs_off_t off);
static lfs_soff_t fs_dir_tell_wrapper(lfs_dir_t *dir);
static int fs_dir_rewind_wrapper(lfs_dir_t *dir);
static int fs_fs_stat_wrapper(struct lfs_fsinfo *fsinfo);
static lfs_ssize_t fs_fs_size_wrapper(void);
static int fs_fs_traverse_wrapper(int (*cb)(void*, lfs_block_t), void *data);
#ifndef LFS_READONLY
static int fs_fs_mkconsistent_wrapper(void);
static int fs_fs_gc_wrapper(void);
static int fs_fs_grow_wrapper(lfs_size_t block_count);
#endif
#if !defined(LFS_READONLY) && defined(LFS_MIGRATE)
static int fs_migrate_wrapper(const struct lfs_config *cfg);
#endif

/* LittleFS operations structure - initialized with wrapper function pointers */
static LittleFSOps_t xLfsOps = {
#ifndef LFS_READONLY
    .remove = fs_remove_wrapper,
    .rename = fs_rename_wrapper,
#endif
    .stat = fs_stat_wrapper,
    .getattr = fs_getattr_wrapper,
#ifndef LFS_READONLY
    .setattr = fs_setattr_wrapper,
    .removeattr = fs_removeattr_wrapper,
#endif
#ifndef LFS_NO_MALLOC
    .file_open = fs_file_open_wrapper,
#endif
    .file_opencfg = fs_file_opencfg_wrapper,
    .file_close = fs_file_close_wrapper,
    .file_sync = fs_file_sync_wrapper,
    .file_read = fs_file_read_wrapper,
#ifndef LFS_READONLY
    .file_write = fs_file_write_wrapper,
#endif
    .file_seek = fs_file_seek_wrapper,
#ifndef LFS_READONLY
    .file_truncate = fs_file_truncate_wrapper,
#endif
    .file_tell = fs_file_tell_wrapper,
    .file_rewind = fs_file_rewind_wrapper,
    .file_size = fs_file_size_wrapper,
#ifndef LFS_READONLY
    .mkdir = fs_mkdir_wrapper,
#endif
    .dir_open = fs_dir_open_wrapper,
    .dir_close = fs_dir_close_wrapper,
    .dir_read = fs_dir_read_wrapper,
    .dir_seek = fs_dir_seek_wrapper,
    .dir_tell = fs_dir_tell_wrapper,
    .dir_rewind = fs_dir_rewind_wrapper,
    .fs_stat = fs_fs_stat_wrapper,
    .fs_size = fs_fs_size_wrapper,
    .fs_traverse = fs_fs_traverse_wrapper,
#ifndef LFS_READONLY
    .fs_mkconsistent = fs_fs_mkconsistent_wrapper,
    .fs_gc = fs_fs_gc_wrapper,
    .fs_grow = fs_fs_grow_wrapper,
#endif
#if !defined(LFS_READONLY) && defined(LFS_MIGRATE)
    .migrate = fs_migrate_wrapper,
#endif
};

/**
 * @brief Initialize the file system module with littlefs operations
 *
 * @param fs_kind File system kind
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xFileSystemInit(FSKind_t fs_kind) {
    /* Check if already initialized */
    if (xGlobalFileSystem.xInitialized == pdTRUE) {
        return pdFAIL;
    }

    /* Currently only support littlefs */
    if (fs_kind != lfs) {
        return pdFAIL;
    }

    /* Allocate memory for lfs_t structure */
    lfs_t *pxLfs = (lfs_t *)pvPortMalloc(sizeof(lfs_t));
    if (pxLfs == NULL) {
        return pdFAIL;
    }

    /* Clear the allocated memory */
    for (size_t i = 0; i < sizeof(lfs_t); i++) {
        ((uint8_t *)pxLfs)[i] = 0;
    }

    /* Initialize littlefs */
    if (init_littlefs(pxLfs) != 0) {
        vPortFree(pxLfs);
        return pdFAIL;
    }

    /* Setup global file system structure */
    xGlobalFileSystem.fs_ops = (void *)&xLfsOps;
    xGlobalFileSystem.pvFsContext = (void *)pxLfs;
    xGlobalFileSystem.filesystem = lfs;
    xGlobalFileSystem.xInitialized = pdTRUE;
    xGlobalFileSystem.xMounted = pdTRUE;

    return pdPASS;
}

/**
 * @brief Get littlefs operations structure
 *
 * @return Pointer to LittleFS operations structure
 */
LittleFSOps_t *pxGetLfsOps(void) {
    if (xGlobalFileSystem.xInitialized == pdFALSE) {
        return NULL;
    }
    return (LittleFSOps_t *)xGlobalFileSystem.fs_ops;
}

/**
 * @brief Get the global file system instance
 *
 * @return Pointer to global file system structure
 */
FileSystem_t *pxGetFileSystem(void) {
    if (xGlobalFileSystem.xInitialized == pdFALSE) {
        return NULL;
    }
    return &xGlobalFileSystem;
}

/**
 * @brief Deinitialize the file system module
 *
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xFileSystemDeinit(void) {
    if (xGlobalFileSystem.xInitialized == pdFALSE) {
        return pdFAIL;
    }

    /* Unmount if mounted */
    if (xGlobalFileSystem.xMounted == pdTRUE && xGlobalFileSystem.pvFsContext != NULL) {
        lfs_t *pxLfs = (lfs_t *)xGlobalFileSystem.pvFsContext;
        lfs_unmount(pxLfs);
    }

    /* Free the lfs_t structure */
    if (xGlobalFileSystem.pvFsContext != NULL) {
        vPortFree(xGlobalFileSystem.pvFsContext);
        xGlobalFileSystem.pvFsContext = NULL;
    }

    /* Reset global file system structure */
    xGlobalFileSystem.fs_ops = NULL;
    xGlobalFileSystem.xInitialized = pdFALSE;
    xGlobalFileSystem.xMounted = pdFALSE;

    return pdPASS;
}

/*-----------------------------------------------------------
 * WRAPPER FUNCTIONS IMPLEMENTATION
 * These functions get FileSystem_t internally and call the actual littlefs functions
 *----------------------------------------------------------*/

#ifndef LFS_READONLY
static int fs_remove_wrapper(const char *path) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    const char *full_path = fs_build_full_path(path, tmp_path, configMAX_PATH_LEN);
    if (full_path == NULL) return LFS_ERR_INVAL;
    return lfs_remove((lfs_t *)fs->pvFsContext, full_path);
}

static int fs_rename_wrapper(const char *oldpath, const char *newpath) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    
    /* Build full paths for both old and new paths */
    char old_full_path[configMAX_PATH_LEN];
    char new_full_path[configMAX_PATH_LEN];
    
    const char *old_full = fs_build_full_path(oldpath, old_full_path, configMAX_PATH_LEN);
    const char *new_full = fs_build_full_path(newpath, new_full_path, configMAX_PATH_LEN);
    
    if (old_full == NULL || new_full == NULL) return LFS_ERR_INVAL;
    
    return lfs_rename((lfs_t *)fs->pvFsContext, old_full, new_full);
}
#endif

static int fs_stat_wrapper(const char *path, struct lfs_info *info) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    const char *full_path = fs_build_full_path(path, tmp_path, configMAX_PATH_LEN);
    if (full_path == NULL) return LFS_ERR_INVAL;
    return lfs_stat((lfs_t *)fs->pvFsContext, full_path, info);
}

static lfs_ssize_t fs_getattr_wrapper(const char *path,
                                      uint8_t type, void *buffer, lfs_size_t size) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    const char *full_path = fs_build_full_path(path, tmp_path, configMAX_PATH_LEN);
    if (full_path == NULL) return LFS_ERR_INVAL;
    return lfs_getattr((lfs_t *)fs->pvFsContext, full_path, type, buffer, size);
}

#ifndef LFS_READONLY
static int fs_setattr_wrapper(const char *path,
                              uint8_t type, const void *buffer, lfs_size_t size) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    const char *full_path = fs_build_full_path(path, tmp_path, configMAX_PATH_LEN);
    if (full_path == NULL) return LFS_ERR_INVAL;
    return lfs_setattr((lfs_t *)fs->pvFsContext, full_path, type, buffer, size);
}

static int fs_removeattr_wrapper(const char *path, uint8_t type) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    const char *full_path = fs_build_full_path(path, tmp_path, configMAX_PATH_LEN);
    if (full_path == NULL) return LFS_ERR_INVAL;
    return lfs_removeattr((lfs_t *)fs->pvFsContext, full_path, type);
}
#endif

#ifndef LFS_NO_MALLOC
static int fs_file_open_wrapper(lfs_file_t *file,
                                const char *path, int flags) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    const char *full_path = fs_build_full_path(path, tmp_path, configMAX_PATH_LEN);
    if (full_path == NULL) return LFS_ERR_INVAL;
    return lfs_file_open((lfs_t *)fs->pvFsContext, file, full_path, flags);
}
#endif

static int fs_file_opencfg_wrapper(lfs_file_t *file,
                                   const char *path, int flags,
                                   const struct lfs_file_config *config) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    const char *full_path = fs_build_full_path(path, tmp_path, configMAX_PATH_LEN);
    if (full_path == NULL) return LFS_ERR_INVAL;
    return lfs_file_opencfg((lfs_t *)fs->pvFsContext, file, full_path, flags, config);
}

static int fs_file_close_wrapper(lfs_file_t *file) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    return lfs_file_close((lfs_t *)fs->pvFsContext, file);
}

static int fs_file_sync_wrapper(lfs_file_t *file) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    return lfs_file_sync((lfs_t *)fs->pvFsContext, file);
}

static lfs_ssize_t fs_file_read_wrapper(lfs_file_t *file,
                                        void *buffer, lfs_size_t size) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    return lfs_file_read((lfs_t *)fs->pvFsContext, file, buffer, size);
}

#ifndef LFS_READONLY
static lfs_ssize_t fs_file_write_wrapper(lfs_file_t *file,
                                         const void *buffer, lfs_size_t size) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    return lfs_file_write((lfs_t *)fs->pvFsContext, file, buffer, size);
}
#endif

static lfs_soff_t fs_file_seek_wrapper(lfs_file_t *file,
                                       lfs_soff_t off, int whence) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    return lfs_file_seek((lfs_t *)fs->pvFsContext, file, off, whence);
}

#ifndef LFS_READONLY
static int fs_file_truncate_wrapper(lfs_file_t *file, lfs_off_t size) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    return lfs_file_truncate((lfs_t *)fs->pvFsContext, file, size);
}
#endif

static lfs_soff_t fs_file_tell_wrapper(lfs_file_t *file) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    return lfs_file_tell((lfs_t *)fs->pvFsContext, file);
}

static int fs_file_rewind_wrapper(lfs_file_t *file) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    return lfs_file_rewind((lfs_t *)fs->pvFsContext, file);
}

static lfs_soff_t fs_file_size_wrapper(lfs_file_t *file) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    return lfs_file_size((lfs_t *)fs->pvFsContext, file);
}

#ifndef LFS_READONLY
static int fs_mkdir_wrapper(const char *path) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    const char *full_path = fs_build_full_path(path, tmp_path, configMAX_PATH_LEN);
    if (full_path == NULL) return LFS_ERR_INVAL;
    return lfs_mkdir((lfs_t *)fs->pvFsContext, full_path);
}
#endif

static int fs_dir_open_wrapper(lfs_dir_t *dir, const char *path) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    const char *full_path = fs_build_full_path(path, tmp_path, configMAX_PATH_LEN);
    if (full_path == NULL) return LFS_ERR_INVAL;
    return lfs_dir_open((lfs_t *)fs->pvFsContext, dir, full_path);
}

static int fs_dir_close_wrapper(lfs_dir_t *dir) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    return lfs_dir_close((lfs_t *)fs->pvFsContext, dir);
}

static int fs_dir_read_wrapper(lfs_dir_t *dir, struct lfs_info *info) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    return lfs_dir_read((lfs_t *)fs->pvFsContext, dir, info);
}

static int fs_dir_seek_wrapper(lfs_dir_t *dir, lfs_off_t off) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    return lfs_dir_seek((lfs_t *)fs->pvFsContext, dir, off);
}

static lfs_soff_t fs_dir_tell_wrapper(lfs_dir_t *dir) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    return lfs_dir_tell((lfs_t *)fs->pvFsContext, dir);
}

static int fs_dir_rewind_wrapper(lfs_dir_t *dir) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    return lfs_dir_rewind((lfs_t *)fs->pvFsContext, dir);
}

static int fs_fs_stat_wrapper(struct lfs_fsinfo *fsinfo) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    return lfs_fs_stat((lfs_t *)fs->pvFsContext, fsinfo);
}

static lfs_ssize_t fs_fs_size_wrapper(void) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    return lfs_fs_size((lfs_t *)fs->pvFsContext);
}

static int fs_fs_traverse_wrapper(int (*cb)(void*, lfs_block_t), void *data) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    return lfs_fs_traverse((lfs_t *)fs->pvFsContext, cb, data);
}

#ifndef LFS_READONLY
static int fs_fs_mkconsistent_wrapper(void) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    return lfs_fs_mkconsistent((lfs_t *)fs->pvFsContext);
}

static int fs_fs_gc_wrapper(void) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    return lfs_fs_gc((lfs_t *)fs->pvFsContext);
}

static int fs_fs_grow_wrapper(lfs_size_t block_count) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    return lfs_fs_grow((lfs_t *)fs->pvFsContext, block_count);
}
#endif

#if !defined(LFS_READONLY) && defined(LFS_MIGRATE)
static int fs_migrate_wrapper(const struct lfs_config *cfg) {
    FileSystem_t *fs = pxGetFileSystem();
    if (fs == NULL || fs->pvFsContext == NULL) return LFS_ERR_INVAL;
    return lfs_migrate((lfs_t *)fs->pvFsContext, cfg);
}
#endif

#endif /* configUSE_LITTLEFS */
