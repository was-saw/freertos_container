#include "FreeRTOS.h"

#ifndef INC_FILESYSTEM_H
#define INC_FILESYSTEM_H

#ifndef INC_FREERTOS_H
#error "include FreeRTOS.h must appear in source files before include chroot.h"
#endif

#ifdef configUSE_LITTLEFS
#include "lfs.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------
 * MACROS AND DEFINITIONS
 *----------------------------------------------------------*/

/* File system configuration */
#ifndef configUSE_FILESYSTEM
#define configUSE_FILESYSTEM 0
#endif

#ifndef configMAX_PATH_LEN
#define configMAX_PATH_LEN 256
#endif

#ifndef configMAX_FILENAME_LEN
#define configMAX_FILENAME_LEN 255
#endif

/* File system type definitions */
typedef void *FSHandle_t;
typedef void *FSFileHandle_t;
typedef void *FSDirHandle_t;

/* File system types */
typedef int32_t  fs_ssize_t;
typedef int32_t  fs_soff_t;
typedef uint32_t fs_size_t;
typedef uint32_t fs_off_t;
typedef uint32_t fs_block_t;

typedef enum FSKind { lfs } FSKind_t;

/* Forward declaration for FileSystem_t */
typedef struct xFILE_SYSTEM FileSystem_t;

/*-----------------------------------------------------------
 * FILE SYSTEM OPERATIONS STRUCTURE
 *----------------------------------------------------------*/

#ifdef configUSE_LITTLEFS
/* LittleFS operations structure - contains all littlefs functions */
typedef struct xLFS_OPS {
    /* General operations */
#ifndef LFS_READONLY
    // Removes a file or directory
    //
    // If removing a directory, the directory must be empty.
    // Returns a negative error code on failure.
    int (*remove)(const char *path);
#endif

#ifndef LFS_READONLY
    // Rename or move a file or directory
    //
    // If the destination exists, it must match the source in type.
    // If the destination is a directory, the directory must be empty.
    //
    // Returns a negative error code on failure.
    int (*rename)(const char *oldpath, const char *newpath);
#endif

    // Find info about a file or directory
    //
    // Fills out the info structure, based on the specified file or directory.
    // Returns a negative error code on failure.
    int (*stat)(const char *path, struct lfs_info *info);

    // Get a custom attribute
    //
    // Custom attributes are uniquely identified by an 8-bit type and limited
    // to LFS_ATTR_MAX bytes. When read, if the stored attribute is smaller than
    // the buffer, it will be padded with zeros. If the stored attribute is larger,
    // then it will be silently truncated. If no attribute is found, the error
    // LFS_ERR_NOATTR is returned and the buffer is filled with zeros.
    //
    // Returns the size of the attribute, or a negative error code on failure.
    // Note, the returned size is the size of the attribute on disk, irrespective
    // of the size of the buffer. This can be used to dynamically allocate a buffer
    // or check for existence.
    lfs_ssize_t (*getattr)(const char *path,
                           uint8_t type, void *buffer, lfs_size_t size);

#ifndef LFS_READONLY
    // Set custom attributes
    //
    // Custom attributes are uniquely identified by an 8-bit type and limited
    // to LFS_ATTR_MAX bytes. If an attribute is not found, it will be
    // implicitly created.
    //
    // Returns a negative error code on failure.
    int (*setattr)(const char *path,
                   uint8_t type, const void *buffer, lfs_size_t size);
#endif

#ifndef LFS_READONLY
    // Removes a custom attribute
    //
    // If an attribute is not found, nothing happens.
    //
    // Returns a negative error code on failure.
    int (*removeattr)(const char *path, uint8_t type);
#endif

    /* File operations */
#ifndef LFS_NO_MALLOC
    // Open a file
    //
    // The mode that the file is opened in is determined by the flags, which
    // are values from the enum lfs_open_flags that are bitwise-ored together.
    //
    // Returns a negative error code on failure.
    int (*file_open)(lfs_file_t *file,
                     const char *path, int flags);
#endif

    // Open a file with extra configuration
    //
    // The mode that the file is opened in is determined by the flags, which
    // are values from the enum lfs_open_flags that are bitwise-ored together.
    //
    // The config struct provides additional config options per file as described
    // above. The config struct must remain allocated while the file is open, and
    // the config struct must be zeroed for defaults and backwards compatibility.
    //
    // Returns a negative error code on failure.
    int (*file_opencfg)(lfs_file_t *file,
                        const char *path, int flags,
                        const struct lfs_file_config *config);

    // Close a file
    //
    // Any pending writes are written out to storage as though
    // sync had been called and releases any allocated resources.
    //
    // Returns a negative error code on failure.
    int (*file_close)(lfs_file_t *file);

    // Synchronize a file on storage
    //
    // Any pending writes are written out to storage.
    // Returns a negative error code on failure.
    int (*file_sync)(lfs_file_t *file);

    // Read data from file
    //
    // Takes a buffer and size indicating where to store the read data.
    // Returns the number of bytes read, or a negative error code on failure.
    lfs_ssize_t (*file_read)(lfs_file_t *file,
                             void *buffer, lfs_size_t size);

#ifndef LFS_READONLY
    // Write data to file
    //
    // Takes a buffer and size indicating the data to write. The file will not
    // actually be updated on the storage until either sync or close is called.
    //
    // Returns the number of bytes written, or a negative error code on failure.
    lfs_ssize_t (*file_write)(lfs_file_t *file,
                              const void *buffer, lfs_size_t size);
#endif

    // Change the position of the file
    //
    // The change in position is determined by the offset and whence flag.
    // Returns the new position of the file, or a negative error code on failure.
    lfs_soff_t (*file_seek)(lfs_file_t *file,
                            lfs_soff_t off, int whence);

#ifndef LFS_READONLY
    // Truncates the size of the file to the specified size
    //
    // Returns a negative error code on failure.
    int (*file_truncate)(lfs_file_t *file, lfs_off_t size);
#endif

    // Return the position of the file
    //
    // Equivalent to lfs_file_seek(lfs, file, 0, LFS_SEEK_CUR)
    // Returns the position of the file, or a negative error code on failure.
    lfs_soff_t (*file_tell)(lfs_file_t *file);

    // Change the position of the file to the beginning of the file
    //
    // Equivalent to lfs_file_seek(lfs, file, 0, LFS_SEEK_SET)
    // Returns a negative error code on failure.
    int (*file_rewind)(lfs_file_t *file);

    // Return the size of the file
    //
    // Similar to lfs_file_seek(lfs, file, 0, LFS_SEEK_END)
    // Returns the size of the file, or a negative error code on failure.
    lfs_soff_t (*file_size)(lfs_file_t *file);

    /* Directory operations */
#ifndef LFS_READONLY
    // Create a directory
    //
    // Returns a negative error code on failure.
    int (*mkdir)(const char *path);
#endif

    // Open a directory
    //
    // Once open a directory can be used with read to iterate over files.
    // Returns a negative error code on failure.
    int (*dir_open)(lfs_dir_t *dir, const char *path);

    // Close a directory
    //
    // Releases any allocated resources.
    // Returns a negative error code on failure.
    int (*dir_close)(lfs_dir_t *dir);

    // Read an entry in the directory
    //
    // Fills out the info structure, based on the specified file or directory.
    // Returns a positive value on success, 0 at the end of directory,
    // or a negative error code on failure.
    int (*dir_read)(lfs_dir_t *dir, struct lfs_info *info);

    // Change the position of the directory
    //
    // The new off must be a value previous returned from tell and specifies
    // an absolute offset in the directory seek.
    //
    // Returns a negative error code on failure.
    int (*dir_seek)(lfs_dir_t *dir, lfs_off_t off);

    // Return the position of the directory
    //
    // The returned offset is only meant to be consumed by seek and may not make
    // sense, but does indicate the current position in the directory iteration.
    //
    // Returns the position of the directory, or a negative error code on failure.
    lfs_soff_t (*dir_tell)(lfs_dir_t *dir);

    // Change the position of the directory to the beginning of the directory
    //
    // Returns a negative error code on failure.
    int (*dir_rewind)(lfs_dir_t *dir);

    /* Filesystem-level operations */

    // Find on-disk info about the filesystem
    //
    // Fills out the fsinfo structure based on the filesystem found on-disk.
    // Returns a negative error code on failure.
    int (*fs_stat)(struct lfs_fsinfo *fsinfo);

    // Finds the current size of the filesystem
    //
    // Note: Result is best effort. If files share COW structures, the returned
    // size may be larger than the filesystem actually is.
    //
    // Returns the number of allocated blocks, or a negative error code on failure.
    lfs_ssize_t (*fs_size)(void);

    // Traverse through all blocks in use by the filesystem
    //
    // The provided callback will be called with each block address that is
    // currently in use by the filesystem. This can be used to determine which
    // blocks are in use or how much of the storage is available.
    //
    // Returns a negative error code on failure.
    int (*fs_traverse)(int (*cb)(void*, lfs_block_t), void *data);

#ifndef LFS_READONLY
    // Attempt to make the filesystem consistent and ready for writing
    //
    // Calling this function is not required, consistency will be implicitly
    // enforced on the first operation that writes to the filesystem, but this
    // function allows the work to be performed earlier and without other
    // filesystem changes.
    //
    // Returns a negative error code on failure.
    int (*fs_mkconsistent)(void);
#endif

#ifndef LFS_READONLY
    // Attempt any janitorial work
    //
    // This currently:
    // 1. Calls mkconsistent if not already consistent
    // 2. Compacts metadata > compact_thresh
    // 3. Populates the block allocator
    //
    // Though additional janitorial work may be added in the future.
    //
    // Calling this function is not required, but may allow the offloading of
    // expensive janitorial work to a less time-critical code path.
    //
    // Returns a negative error code on failure. Accomplishing nothing is not
    // an error.
    int (*fs_gc)(void);
#endif

#ifndef LFS_READONLY
    // Grows the filesystem to a new size, updating the superblock with the new
    // block count.
    //
    // If LFS_SHRINKNONRELOCATING is defined, this function will also accept
    // block_counts smaller than the current configuration, after checking
    // that none of the blocks that are being removed are in use.
    // Note that littlefs's pseudorandom block allocation means that
    // this is very unlikely to work in the general case.
    //
    // Returns a negative error code on failure.
    int (*fs_grow)(lfs_size_t block_count);
#endif

#if !defined(LFS_READONLY) && defined(LFS_MIGRATE)
    // Attempts to migrate a previous version of littlefs
    //
    // Behaves similarly to the lfs_format function. Attempts to mount
    // the previous version of littlefs and update the filesystem so it can be
    // mounted with the current version of littlefs.
    //
    // Requires a littlefs object and config struct. This clobbers the littlefs
    // object, and does not leave the filesystem mounted. The config struct must
    // be zeroed for defaults and backwards compatibility.
    //
    // Returns a negative error code on failure.
    int (*migrate)(const struct lfs_config *cfg);
#endif
} LittleFSOps_t;
#endif

/* Global file system structure */
struct xFILE_SYSTEM {
    void      *fs_ops;      /* LittleFS operations - contains all littlefs functions */
    void      *pvFsContext; /* File system context (e.g., lfs_t instance) */
    FSKind_t   filesystem;
    BaseType_t xInitialized; /* Initialization flag */
    BaseType_t xMounted;     /* Mount status */
};

/*-----------------------------------------------------------
 * FILE SYSTEM API FUNCTIONS
 *----------------------------------------------------------*/

#if (configUSE_FILESYSTEM == 1)

/**
 * file_system.h
 * @brief Initialize the file system module with littlefs operations
 *
 * @param fs_kind File system kind
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xFileSystemInit(FSKind_t fs_kind);

/**
 * file_system.h
 * @brief Get the global file system instance
 * Users can access file_system.fs_ops and pvFsContext to use littlefs
 *
 * @return Pointer to global file system structure, or NULL if not initialized
 */
FileSystem_t *pxGetFileSystem(void);

/**
 * file_system.h
 * @brief Deinitialize the file system module
 *
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xFileSystemDeinit(void);

#ifdef configUSE_LITTLEFS
/**
 * file_system.h
 * @brief Get littlefs operations structure with file system limitations
 *
 * @return Pointer to LittleFS operations structure
 */
LittleFSOps_t *pxGetLfsOps(void);
#endif

#else /* configUSE_FILESYSTEM == 0 */

/* When file system is disabled, provide empty macros */
#define xFileSystemInit(fs_kind) pdFAIL
#define pxGetFileSystem() NULL
#define xFileSystemDeinit() pdFAIL
#define pxGetLfsOps() NULL

#endif /* configUSE_FILESYSTEM */

#ifdef __cplusplus
}
#endif

#endif /* INC_FILESYSTEM_H */
