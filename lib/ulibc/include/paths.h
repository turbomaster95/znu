#ifndef _USERLIBC_PATHS_H
#define _USERLIBC_PATHS_H

// Default search paths for executables
#define _PATH_DEFPATH   "/usr/bin:/bin"
#define _PATH_STDPATH   "/usr/bin:/bin:/usr/sbin:/sbin"

// Core system directories
#define _PATH_BSHELL    "/bin/sh"
#define _PATH_CONSOLE   "/dev/console"
#define _PATH_DEV       "/dev/"
#define _PATH_DEVNULL   "/dev/null"
#define _PATH_KLOG      "/proc/kmsg"
#define _PATH_MNTTAB    "/etc/fstab"
#define _PATH_MOUNTED   "/etc/mtab"
#define _PATH_SENDMAIL  "/usr/sbin/sendmail"
#define _PATH_SH        "/bin/sh"
#define _PATH_TMP       "/tmp/"
#define _PATH_VARDB     "/var/db/"
#define _PATH_VARRUN    "/var/run/"
#define _PATH_VARTMP    "/var/tmp/"

#endif // _USERLIBC_PATHS_H
