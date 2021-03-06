/*
  Dokan : user-mode file system library for Windows

  Copyright (C) 2015 - 2016 Adrien J. <liryna.stark@gmail.com> and Maxime C. <maxime@islog.com>
  Copyright (C) 2007 - 2011 Hiroki Asakawa <info@dokan-dev.net>

  http://dokan-dev.github.io

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free
Software Foundation; either version 3 of the License, or (at your option) any
later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DOKAN_H_
#define DOKAN_H_

#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>

#include "fileinfo.h"
#include "public.h"

#define DOKAN_DRIVER_NAME L"dokan" DOKAN_MAJOR_API_VERSION L".sys"
#define DOKAN_NP_NAME L"Dokan" DOKAN_MAJOR_API_VERSION

#ifdef _EXPORTING
#define DOKANAPI /*__declspec(dllexport)*/                                     \
  __stdcall      // exports defined in dokan.def
#else
#define DOKANAPI __declspec(dllimport) __stdcall
#endif

#define DOKAN_CALLBACK __stdcall

#ifdef __cplusplus
extern "C" {
#endif

/** @file */

/**
 * \defgroup Dokan Dokan
 * \brief Dokan Library const and methodes
 */
/** @{ */

/** The current Dokan version (ver 1.0.0). \ref DOKAN_OPTIONS.Version */
#define DOKAN_VERSION 100
/** Minimum Dokan version (ver 1.0.0) accepted. */
#define DOKAN_MINIMUM_COMPATIBLE_VERSION 100
/** Maximum number of dokan instances.*/
#define DOKAN_MAX_INSTANCES 32

/** @} */

/**
 * \defgroup DOKAN_OPTION DOKAN_OPTION
 * \brief All DOKAN_OPTION flags used in DOKAN_OPTIONS.Options
 * \see DOKAN_FILE_INFO
 */
/** @{ */

/** Enable ouput debug message */
#define DOKAN_OPTION_DEBUG 1
/** Enable ouput debug message to stderr */
#define DOKAN_OPTION_STDERR 2
/** Use alternate stream */
#define DOKAN_OPTION_ALT_STREAM 4
/** Enable mount drive as write-protected */
#define DOKAN_OPTION_WRITE_PROTECT 8
/** Use network drive - Dokan network provider need to be installed */
#define DOKAN_OPTION_NETWORK 16
/** Use removable drive */
#define DOKAN_OPTION_REMOVABLE 32
/** Use mount manager */
#define DOKAN_OPTION_MOUNT_MANAGER 64
/** Mount the drive on current session only */
#define DOKAN_OPTION_CURRENT_SESSION 128
/** Enable Lockfile/Unlockfile operations. Otherwise Dokan will take care of it */
#define DOKAN_OPTION_FILELOCK_USER_MODE 256

/** @} */

/**
 * \struct DOKAN_OPTIONS
 * \brief Dokan mount options used to describe dokan device behaviour.
 */
typedef struct _DOKAN_OPTIONS {
  /** Version of the dokan features requested (Version "123" is equal to Dokan version 1.2.3) */
  USHORT Version;
  /** Number of threads to be used internally by Dokan library. More thread will handle more event at the same time */
  USHORT ThreadCount;
  /** Features enable for the mount. \ref DOKAN_OPTION */
  ULONG Options;
  /** FileSystem can store anything here */
  ULONG64 GlobalContext;
  /** Mount point. Can be "M:\" (drive letter) or "C:\mount\dokan" (path in NTFS) */
  LPCWSTR MountPoint;
  /** UNC name used for network volume */
  LPCWSTR UNCName;
  /** Max timeout in milliseconds of each request before dokan give up */
  ULONG Timeout;
  /** Allocation Unit Size of the volume. This will behave on the file size */
  ULONG AllocationUnitSize;
  /** Sector Size of the volume. This will behave on the file size */
  ULONG SectorSize;
} DOKAN_OPTIONS, *PDOKAN_OPTIONS;

/**
 * \struct DOKAN_FILE_INFO
 * \brief Dokan file information on the current operation.
 */
typedef struct _DOKAN_FILE_INFO {
  /**
   * Context that can be used to carry information between operation.
   * The Context can carry whatever type like HANDLE, struct, int,
   * internal reference that will help the implementation understand the request context of the event.
   */
  ULONG64 Context;
  /** Used internally, never modify */
  ULONG64 DokanContext;
  /** A pointer to DOKAN_OPTIONS which was passed to DokanMain. */
  PDOKAN_OPTIONS DokanOptions;
  /**
   * Process id for the thread that originally requested a given I/O operation
   */
  ULONG ProcessId;
  /**
   * Requesting a directory file
   * IsDirectory has to be set in CreateFile is the file appear to be a folder.
   */
  UCHAR IsDirectory;
  /** Flag if the file has to be delete during DOKAN_OPERATIONS.Cleanup event. */
  UCHAR DeleteOnClose;
  /** Read or write is paging IO. */
  UCHAR PagingIo;
  /** Read or write is synchronous IO. */
  UCHAR SynchronousIo;
  /** Read or write directly from data source without cache */
  UCHAR Nocache;
  /**  If true, write to the current end of file instead of Offset parameter. */
  UCHAR WriteToEndOfFile;
} DOKAN_FILE_INFO, *PDOKAN_FILE_INFO;

/**
 * \brief FillFindData Used to add an entry in FindFiles operation
 * \return 1 if buffer is full, otherwise 0 (currently it never returns 1)
 */
typedef int(WINAPI *PFillFindData)(PWIN32_FIND_DATAW, PDOKAN_FILE_INFO);

/**
 * \brief FillFindStreamData Used to add an entry in FindStreams
 * \return 1 if buffer is full, otherwise 0 (currently it never returns 1)
 */
typedef int(WINAPI *PFillFindStreamData)(PWIN32_FIND_STREAM_DATA,
                                         PDOKAN_FILE_INFO);

// clang-format off

/**
 * \struct DOKAN_OPERATIONS
 * \brief Dokan API callbacks interface
 *
 * DOKAN_OPERATIONS is a struct of callbacks that describe all Dokan API operation
 * that will be called when Windows acces to the filesystem.
 *
 * If an error occurs, return NTSTATUS (https://support.microsoft.com/en-us/kb/113996).
 * Win32 Error can be converted to NTSTATUS with DokanNtStatusFromWin32
 *
 * All this callbacks can be set to NULL or return STATUS_NOT_IMPLEMENTED
 * if you dont want to support one of them. Be aware that returning such value to important callbacks
 * such CreateFile/ReadFile/... would make the filesystem not working or unstable.
 */
typedef struct _DOKAN_OPERATIONS {
  /**
  * \brief CreateFile Dokan API callback
  *
  * CreateFile is called each time a request is made on a file.
  *
  * In case OPEN_ALWAYS & CREATE_ALWAYS are opening successfully a already
  * existing file, you have to \code SetLastError(ERROR_ALREADY_EXISTS) \endcode
  *
  * If the file is a directory, CreateFile is also called.
  * In this case, CreateFile should return STATUS_SUCCESS when that directory
  * can be opened and DOKAN_FILE_INFO.IsDirectory has to be set to TRUE.
  *
  * DOKAN_FILE_INFO.Context can be use to store Data (like HANDLE)
  * that can be retrieved in all other request related to the Context
  *
  * See ZwCreateFile for more information about the parameters of this callback.
  * https://msdn.microsoft.com/en-us/library/windows/hardware/ff566424(v=vs.85).aspx
  *
  * \see DokanMapKernelToUserCreateFileFlags
  *
  * \param FileName File path requested by the Kernel on the FileSystem.
  * \param SecurityContext SecurityContext, see https://msdn.microsoft.com/en-us/library/windows/hardware/ff550613(v=vs.85).aspx
  * \param DesiredAccess Specifies an ACCESS_MASK value that determines the requested access to the object.
  * \param FileAttributes Specifies one or more FILE_ATTRIBUTE_XXX flags, which represent the file attributes to set if you create or overwrite a file.
  * \param ShareAccess Type of share access, which is specified as zero or any combination of FILE_SHARE_* flags.
  * \param CreateDisposition Specifies the action to perform if the file does or does not exist.
  * \param CreateOptions Specifies the options to apply when the driver creates or opens the file.
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *ZwCreateFile)(LPCWSTR FileName,
      PDOKAN_IO_SECURITY_CONTEXT SecurityContext,
      ACCESS_MASK DesiredAccess,
      ULONG FileAttributes,
      ULONG ShareAccess,
      ULONG CreateDisposition,
      ULONG CreateOptions,
      PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief Cleanup Dokan API callback
  *
  * Cleanup request before CloseFile is called.
  *
  * When DOKAN_FILE_INFO.DeleteOnClose is TRUE, you must delete the file in Cleanup.
  * See DeleteFile documentation for explanation.
  *
  * \param FileName File path requested by the Kernel on the FileSystem.
  * \param DokanFileInfo Information about the file or directory.
  */
  void(DOKAN_CALLBACK *Cleanup)(LPCWSTR FileName,
                                PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief CloseFile Dokan API callback
  *
  * Clean remaining Context
  *
  * \param FileName File path requested by the Kernel on the FileSystem.
  * \param DokanFileInfo Information about the file or directory.
  */
  void(DOKAN_CALLBACK *CloseFile)(LPCWSTR FileName,
    PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief ReadFile Dokan API callback
  *
  * ReadFile callback on the file previously opened in CreateFile
  * It can be called by different thread at the same time.
  * Therefor the read/context has to be thread safe.
  *
  * \param FileName File path requested by the Kernel on the FileSystem.
  * \param Buffer Read buffer that has to be fill with the read result.
  * \param BufferLength Buffer length and also the read size to proceed.
  * \param ReadLength Total data size that has been read.
  * \param Offset Offset from where the read has to be proceed.
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *ReadFile)(LPCWSTR FileName,
    LPVOID Buffer,
    DWORD BufferLength,
    LPDWORD ReadLength,
    LONGLONG Offset,
    PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief WriteFile Dokan API callback
  *
  * WriteFile callback on the file previously opened in CreateFile
  * It can be called by different thread at the same time.
  * Therefor the write/context has to be thread safe.
  *
  * \param FileName File path requested by the Kernel on the FileSystem.
  * \param Buffer Data that has to be written.
  * \param NumberOfBytesToWrite Buffer length and also the write size to proceed.
  * \param NumberOfBytesWritten Total byte that has been write.
  * \param Offset Offset from where the write has to be proceed.
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *WriteFile)(LPCWSTR FileName,
    LPCVOID Buffer,
    DWORD NumberOfBytesToWrite,
    LPDWORD NumberOfBytesWritten,
    LONGLONG Offset,
    PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief FlushFileBuffers Dokan API callback
  *
  * Clears buffers for this context and causes any buffered data to be written to the file.
  *
  * \param FileName File path requested by the Kernel on the FileSystem.
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *FlushFileBuffers)(LPCWSTR FileName,
    PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief GetFileInformation Dokan API callback
  *
  * Get specific informations on a file.
  *
  * \param FileName File path requested by the Kernel on the FileSystem.
  * \param Buffer BY_HANDLE_FILE_INFORMATION struct to fill.
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *GetFileInformation)(LPCWSTR FileName,
    LPBY_HANDLE_FILE_INFORMATION Buffer,
    PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief FindFiles Dokan API callback
  *
  * List all files in the path requested
  * FindFilesWithPattern is checking first. If it is not implemented or
  * returns STATUS_NOT_IMPLEMENTED, then FindFiles is called, if implemented.
  *
  * \param FileName File path requested by the Kernel on the FileSystem.
  * \param FillFindData Callback that has to be called with PWIN32_FIND_DATAW that contain file information.
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *FindFiles)(LPCWSTR FileName,
    PFillFindData FillFindData,
    PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief FindFilesWithPattern Dokan API callback
  *
  * Same as FindFiles but with a search pattern.
  *
  * \param PathName Path requested by the Kernel on the FileSystem.
  * \param SearchPattern Search pattern.
  * \param FillFindData Callback that has to be called with PWIN32_FIND_DATAW that contain file information.
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *FindFilesWithPattern)(LPCWSTR PathName,
    LPCWSTR SearchPattern,
    PFillFindData FillFindData,
    PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief SetFileAttributes Dokan API callback
  *
  * Set file attributes on a specific file
  *
  * \param FileName File path requested by the Kernel on the FileSystem.
  * \param FileAttributes FileAttributes to set on file.
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *SetFileAttributes)(LPCWSTR FileName,
    DWORD FileAttributes,
    PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief SetFileTime Dokan API callback
  *
  * Set file attributes on a specific file
  *
  * \param FileName File path requested by the Kernel on the FileSystem.
  * \param CreationTime Creation FILETIME.
  * \param LastAccessTime LastAccess FILETIME.
  * \param LastWriteTime LastWrite FILETIME.
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *SetFileTime)(LPCWSTR FileName,
    CONST FILETIME *CreationTime,
    CONST FILETIME *LastAccessTime,
    CONST FILETIME *LastWriteTime,
    PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief DeleteFile Dokan API callback
  *
  * You should not delete the file on DeleteFile, but instead
  * you must only check whether you can delete the file or not,
  * and return STATUS_SUCCESS (when you can delete it) or appropriate error
  * codes such as STATUS_ACCESS_DENIED or STATUS_OBJECT_NAME_NOT_FOUND.
  *
  * When you return STATUS_SUCCESS, you get a Cleanup call afterwards with
  * FileInfo->DeleteOnClose set to TRUE and only then you have to actually
  * delete the file being closed
  *
  * \param FileName File path requested by the Kernel on the FileSystem.
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *DeleteFile)(LPCWSTR FileName,
    PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief DeleteDirectory Dokan API callback
  *
  * You should not delete the Directory on DeleteDirectory, but instead
  * you must only check whether you can delete the file or not,
  * and return STATUS_SUCCESS (when you can delete it) or appropriate error
  * codes such as STATUS_ACCESS_DENIED, STATUS_OBJECT_PATH_NOT_FOUND,
  * or STATUS_DIRECTORY_NOT_EMPTY.
  *
  * When you return STATUS_SUCCESS, you get a Cleanup call afterwards with
  * FileInfo->DeleteOnClose set to TRUE and only then you have to actually
  * delete the file being closed
  *
  * \param FileName File path requested by the Kernel on the FileSystem.
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *DeleteDirectory)(LPCWSTR FileName,
    PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief MoveFile Dokan API callback
  *
  * Move a file or directory to his new destination
  *
  * \param FileName Source file path to move.
  * \param NewFileName Destination file path.
  * \param ReplaceIfExisting Can replace or not if destination already exist.
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *MoveFile)(LPCWSTR FileName,
    LPCWSTR NewFileName,
    BOOL ReplaceIfExisting,
    PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief SetEndOfFile Dokan API callback
  *
  * SetEndOfFile is used to truncate or extend a file (physical file size).
  *
  * \param FileName File path requested by the Kernel on the FileSystem.
  * \param ByteOffset File length to set.
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *SetEndOfFile)(LPCWSTR FileName,
    LONGLONG ByteOffset,
    PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief SetAllocationSize Dokan API callback
  *
  * SetAllocationSize is used to truncate or extend a file.
  *
  * \param FileName File path requested by the Kernel on the FileSystem.
  * \param AllocSize File length to set.
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *SetAllocationSize)(LPCWSTR FileName,
    LONGLONG AllocSize,
    PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief LockFile Dokan API callback
  *
  * Lock file at a specific offset and data length.
  * This is only used if \ref DOKAN_OPTION_FILELOCK_USER_MODE is enabled.
  *
  * \param FileName File path requested by the Kernel on the FileSystem.
  * \param ByteOffset Offset from where the lock has to be proceed
  * \param Length Data length to lock
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *LockFile)(LPCWSTR FileName,
    LONGLONG ByteOffset,
    LONGLONG Length,
    PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief UnlockFile Dokan API callback
  *
  * Unlock file at a specific offset and data length.
  * This is only used if \ref DOKAN_OPTION_FILELOCK_USER_MODE is enabled.
  *
  * \param FileName File path requested by the Kernel on the FileSystem.
  * \param ByteOffset Offset from where the lock has to be proceed
  * \param Length Data length to lock
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *UnlockFile)(LPCWSTR FileName,
    LONGLONG ByteOffset,
    LONGLONG Length,
    PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief GetDiskFreeSpace Dokan API callback
  *
  * see Win32 API GetDiskFreeSpaceEx
  * https://msdn.microsoft.com/en-us/library/windows/desktop/aa364937(v=vs.85).aspx
  *
  * Neither GetDiskFreeSpace nor GetVolumeInformation
  * save the DOKAN_FILE_INFO.Context
  * Before these methods are called, CreateFile may not be called.
  * (ditto CloseFile and Cleanup)
  *
  * \param FreeBytesAvailable Amount of available space.
  * \param TotalNumberOfBytes Total size of storage space
  * \param TotalNumberOfFreeBytes Amount of free space
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *GetDiskFreeSpace)(PULONGLONG FreeBytesAvailable,
    PULONGLONG TotalNumberOfBytes,
    PULONGLONG TotalNumberOfFreeBytes,
    PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief GetVolumeInformation Dokan API callback
  *
  * see Win32 API GetVolumeInformation
  * https://msdn.microsoft.com/en-us/library/windows/desktop/aa364993(v=vs.85).aspx
  *
  * Neither GetDiskFreeSpace nor GetVolumeInformation
  * save the DokanFileContext->Context.
  * Before these methods are called, CreateFile may not be called.
  * (ditto CloseFile and Cleanup)
  *
  * FILE_READ_ONLY_VOLUME is automatically added to the
  * FileSystemFlags if \ref DOKAN_OPTION_WRITE_PROTECT was
  * specified in DOKAN_OPTIONS when the volume was mounted.
  *
  * \param VolumeNameBuffer A pointer to a buffer that receives the name of a specified volume.
  * \param VolumeNameSize The length of a volume name buffer.
  * \param VolumeSerialNumber A pointer to a variable that receives the volume serial number.
  * \param MaximumComponentLength A pointer to a variable that receives the maximum length.
  * \param FileSystemFlags A pointer to a variable that receives flags associated with the specified file system.
  * \param FileSystemNameBuffer A pointer to a buffer that receives the name of the file system.
  * \param FileSystemNameSize The length of the file system name buffer.
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *GetVolumeInformation)(LPWSTR VolumeNameBuffer,
    DWORD VolumeNameSize,
    LPDWORD VolumeSerialNumber,
    LPDWORD MaximumComponentLength,
    LPDWORD FileSystemFlags,
    LPWSTR FileSystemNameBuffer,
    DWORD FileSystemNameSize,
    PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief Mounted Dokan API callback
  *
  * Mount is called when Dokan succeed to mount the volume.
  *
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *Mounted)(PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief Unmounted Dokan API callback
  *
  * Unmounted is called when Dokan is unmounting the volume.
  *
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *Unmounted)(PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief GetFileSecurity Dokan API callback
  *
  * Get ACL information on the requested file.
  *
  * see Win32 API GetFileSecurity
  * https://msdn.microsoft.com/en-us/library/windows/desktop/aa446639(v=vs.85).aspx
  * 
  * Return STATUS_BUFFER_OVERFLOW if buffer size is too small.
  *
  * Suported since 0.6.0. You must specify the version at
  * DOKAN_OPTIONS.Version.
  *
  * \param FileName File path requested by the Kernel on the FileSystem.
  * \param SecurityInformation A SECURITY_INFORMATION value that identifies the security information being requested.
  * \param SecurityDescriptor A pointer to a buffer that receives a copy of the security descriptor of the requested file.
  * \param BufferLength Specifies the size, in bytes, of the buffer.
  * \param LengthNeeded A pointer to the variable that receives the number of bytes necessary to store the complete security descriptor.
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *GetFileSecurity)(LPCWSTR FileName,
    PSECURITY_INFORMATION SecurityInformation,
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    ULONG BufferLength,
    PULONG LengthNeeded,
    PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief SetFileSecurity Dokan API callback
  *
  * Set ACL information on the requested file.
  *
  * see Win32 API SetFileSecurity
  * https://msdn.microsoft.com/en-us/library/windows/desktop/aa379577(v=vs.85).aspx
  *
  * Suported since 0.6.0. You must specify the version at
  * DOKAN_OPTIONS.Version.
  *
  * \param FileName File path requested by the Kernel on the FileSystem.
  * \param SecurityInformation Structure that identifies the contents of the security descriptor pointed by SecurityDescriptor param.
  * \param SecurityDescriptor A pointer to a SECURITY_DESCRIPTOR structure.
  * \param BufferLength Specifies the size, in bytes, of the buffer.
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *SetFileSecurity)(LPCWSTR FileName,
    PSECURITY_INFORMATION SecurityInformation,
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    ULONG BufferLength,
    PDOKAN_FILE_INFO DokanFileInfo);

  /**
  * \brief FindStreams Dokan API callback
  *
  * Retrieve all NTFS Streams informations on the file.
  * This is only called if \ref DOKAN_OPTION_ALT_STREAM is enabled.
  *
  * Supported since 0.8.0. You must specify the version at
  * DOKAN_OPTIONS.Version.
  *
  * \param FileName File path requested by the Kernel on the FileSystem.
  * \param FillFindStreamData Callback that has to be called with PWIN32_FIND_STREAM_DATA that contain stream information.
  * \param DokanFileInfo Information about the file or directory.
  * \return STATUS_SUCCESS on success or NTSTATUS appropriate to the request result.
  */
  NTSTATUS(DOKAN_CALLBACK *FindStreams)(LPCWSTR FileName,
    PFillFindStreamData FillFindStreamData,
    PDOKAN_FILE_INFO DokanFileInfo);

} DOKAN_OPERATIONS, *PDOKAN_OPERATIONS;

// clang-format on

typedef struct _DOKAN_CONTROL {
  ULONG Type;
  WCHAR MountPoint[MAX_PATH];
  WCHAR UNCName[64];
  WCHAR DeviceName[64];
  PVOID DeviceObject;
} DOKAN_CONTROL, *PDOKAN_CONTROL;

/**
 * \defgroup DokanMain
 * \brief DokanMain returns error codes
 */
/** @{ */

/** Dokan mount succeed */
#define DOKAN_SUCCESS 0
/** Dokan mount error */
#define DOKAN_ERROR -1
/** Dokan mount failed - Bad drive letter */
#define DOKAN_DRIVE_LETTER_ERROR -2
/** Dokan mount failed - Can't install driver  */
#define DOKAN_DRIVER_INSTALL_ERROR -3
/** Dokan mount failed - Driver answer that something is wrong */
#define DOKAN_START_ERROR -4
/**
 * Dokan mount failed.Can't assign a drive letter or mount point
 * Probably already used by another volume
 */
#define DOKAN_MOUNT_ERROR -5
/**
 * Dokan mount failed
 * Mount point is invalid
 */
#define DOKAN_MOUNT_POINT_ERROR -6
/**
 * Dokan mount failed
 * Requested an incompatible version
 */
#define DOKAN_VERSION_ERROR -7

/** @} */

/**
 * \defgroup Dokan Dokan
 */
/** @{ */

/**
 * \brief Mount a new Dokan Volume.
 *
 * This function block until the device is unmount.
 * if the mount fail, it will directly return \ref DokanMain error.
 *
 * \param DokanOptions Dokan option that describe the mount.
 * \param DokanOperations Instance of DOKAN_OPERATIONS that will be called for each request made by the kernel.
 * \return \ref DokanMain status.
 */
int DOKANAPI DokanMain(PDOKAN_OPTIONS DokanOptions,
                       PDOKAN_OPERATIONS DokanOperations);

/**
 * \brief Unmount a dokan device from a driver letter
 *
 * \param DriveLetter Dokan driver letter to unmount.
 * \return True if device was unmount or False in case of failure or device not found.
 */
BOOL DOKANAPI DokanUnmount(WCHAR DriveLetter);

/**
 * \brief Unmount a dokan device from a mount point
 *
 * \param MountPoint Mount point to unmount ("Z", "Z:", "Z:\", "Z:\MyMountPoint").
 * \return True if device was unmount or False in case of failure or device not found.
 */
BOOL DOKANAPI DokanRemoveMountPoint(LPCWSTR MountPoint);

/**
 * \brief Unmount a dokan device from a mount point
 *
 * Same as \ref DokanRemoveMountPoint
 * If Safe is TRUE, will broadcast to all desktop and Shell
 * Safe should not be used during DLL_PROCESS_DETACH
 *
 * \see DokanRemoveMountPoint
 *
 * \param MountPoint Mount point to unmount ("Z", "Z:", "Z:\", "Z:\MyMountPoint").
 * \param Safe Process is not in DLL_PROCESS_DETACH state.
 * \return True if device was unmount or False in case of failure or device not found.
 */
BOOL DOKANAPI DokanRemoveMountPointEx(LPCWSTR MountPoint, BOOL Safe);

/**
 * \brief Checks whether Name can match Expression
 *
 * \param Expression Expression can contain wildcard characters (? and *)
 * \param Name Name to check
 * \param IgnoreCase Case sensitive or not
 * \return result if name match the expression
 */
BOOL DOKANAPI DokanIsNameInExpression(LPCWSTR Expression, LPCWSTR Name,
                                      BOOL IgnoreCase);

/**
 * \brief Get Dokan Version
 * \return Dokan version
 */
ULONG DOKANAPI DokanVersion();

/**
 * \brief Get Dokan Driver Version
 * \return Dokan Kernel version
 */
ULONG DOKANAPI DokanDriverVersion();

/**
 * \brief Extends the time out of the current IO operation in driver.
 *
 * \param Timeout Extended time in milliseconds requested.
 * \param DokanFileInfo \ref DOKAN_FILE_INFO of the operation to extend.
 * \return Extended time allowed.
 */
BOOL DOKANAPI DokanResetTimeout(ULONG Timeout, PDOKAN_FILE_INFO DokanFileInfo);

/**
 * \brief Get the handle to Access Token
 *
 * This method needs be called in CreateFile callback.
 * The caller must call CloseHandle for the returned handle.
 *
 * \param DokanFileInfo \ref DOKAN_FILE_INFO of the operation to extend.
 * \return HANDLE to Access Token.
 */
HANDLE DOKANAPI DokanOpenRequestorToken(PDOKAN_FILE_INFO DokanFileInfo);

BOOL DOKANAPI DokanGetMountPointList(PDOKAN_CONTROL list, ULONG length,
                                     BOOL uncOnly, PULONG nbRead);

/**
 * \brief Convert ZwCreateFile parameters to CreateFile
 *
 * By calling DokanMapKernelToUserCreateFileFlags,
 * you can convert DOKAN_OPERATIONS.ZwCreateFile parameters
 * to CreateFile parameters.
 * https://msdn.microsoft.com/en-us/library/windows/desktop/aa363858(v=vs.85).aspx
 *
 * \param FileAttributes FileAttributes from DOKAN_OPERATIONS.ZwCreateFile.
 * \param CreateOptions CreateOptions from DOKAN_OPERATIONS.ZwCreateFile.
 * \param CreateDisposition CreateDisposition from DOKAN_OPERATIONS.ZwCreateFile.
 * \param outFileAttributesAndFlags New CreateFile dwFlagsAndAttributes
 * \param outCreationDisposition New CreateFile dwCreationDisposition
 */
VOID DOKANAPI DokanMapKernelToUserCreateFileFlags(
    ULONG FileAttributes, ULONG CreateOptions, ULONG CreateDisposition,
    DWORD *outFileAttributesAndFlags, DWORD *outCreationDisposition);

/**
 * \brief Convert WIN32 error to NTSTATUS
 *
 * https://support.microsoft.com/en-us/kb/113996
 *
 * \param Error Win32 Error to convert
 * \return NTSTATUS associate to the ERROR.
 */
NTSTATUS DOKANAPI DokanNtStatusFromWin32(DWORD Error);

/** @} */

#ifdef __cplusplus
}
#endif

#endif // DOKAN_H_
