#ifndef __BITS_MOUNT_H__
#define __BITS_MOUNT_H__

#define MS_RDONLY	(1<<0)
#define MS_NOSUID	(1<<1)
#define MS_NODEV	(1<<2)
#define MS_NOEXEC	(1<<3)
#define MS_SYNCHRONOUS	(1<<4)
#define MS_REMOUNT	(1<<5)
#define MS_MANDLOCK	(1<<6)
#define MS_DIRSYNC	(1<<7)
#define MS_NOATIME	(1<<10)
#define MS_NODIRATIME	(1<<11)
#define MS_BIND		(1<<12)
#define MS_MOVE		(1<<13)
#define MS_REC		(1<<14)
#define MS_SILENT	(1<<15)
#define MS_LAZYTIME	(1<<25)

#define MNT_FORCE	(1<<0)
#define MNT_DETACH	(1<<1)
#define MNT_EXPIRE	(1<<2)
#define UMOUNT_NOFOLLOW	(1<<3)

#endif