/*	$OpenBSD: svr4_syscallargs.h,v 1.30 2000/08/23 19:32:55 fgsch Exp $	*/

/*
 * System call argument lists.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	OpenBSD: syscalls.master,v 1.30 2000/08/23 19:31:35 fgsch Exp 
 */

#define	syscallarg(x)	union { x datum; register_t pad; }

struct svr4_sys_open_args {
	syscallarg(char *) path;
	syscallarg(int) flags;
	syscallarg(int) mode;
};

struct svr4_sys_wait_args {
	syscallarg(int *) status;
};

struct svr4_sys_creat_args {
	syscallarg(char *) path;
	syscallarg(int) mode;
};

struct svr4_sys_execv_args {
	syscallarg(char *) path;
	syscallarg(char **) argp;
};

struct svr4_sys_time_args {
	syscallarg(svr4_time_t *) t;
};

struct svr4_sys_mknod_args {
	syscallarg(char *) path;
	syscallarg(int) mode;
	syscallarg(int) dev;
};

struct svr4_sys_break_args {
	syscallarg(caddr_t) nsize;
};

struct svr4_sys_stat_args {
	syscallarg(char *) path;
	syscallarg(struct svr4_stat *) ub;
};

struct svr4_sys_alarm_args {
	syscallarg(unsigned) sec;
};

struct svr4_sys_fstat_args {
	syscallarg(int) fd;
	syscallarg(struct svr4_stat *) sb;
};

struct svr4_sys_utime_args {
	syscallarg(char *) path;
	syscallarg(struct svr4_utimbuf *) ubuf;
};

struct svr4_sys_access_args {
	syscallarg(char *) path;
	syscallarg(int) flags;
};

struct svr4_sys_nice_args {
	syscallarg(int) prio;
};

struct svr4_sys_kill_args {
	syscallarg(int) pid;
	syscallarg(int) signum;
};

struct svr4_sys_pgrpsys_args {
	syscallarg(int) cmd;
	syscallarg(int) pid;
	syscallarg(int) pgid;
};

struct svr4_sys_times_args {
	syscallarg(struct tms *) tp;
};

struct svr4_sys_signal_args {
	syscallarg(int) signum;
	syscallarg(svr4_sig_t) handler;
};

struct svr4_sys_msgsys_args {
	syscallarg(int) what;
	syscallarg(int) a2;
	syscallarg(int) a3;
	syscallarg(int) a4;
	syscallarg(int) a5;
};

struct svr4_sys_sysarch_args {
	syscallarg(int) op;
	syscallarg(void *) a1;
};

struct svr4_sys_shmsys_args {
	syscallarg(int) what;
	syscallarg(int) a2;
	syscallarg(int) a3;
	syscallarg(int) a4;
};

struct svr4_sys_semsys_args {
	syscallarg(int) what;
	syscallarg(int) a2;
	syscallarg(int) a3;
	syscallarg(int) a4;
	syscallarg(int) a5;
};

struct svr4_sys_ioctl_args {
	syscallarg(int) fd;
	syscallarg(u_long) com;
	syscallarg(caddr_t) data;
};

struct svr4_sys_utssys_args {
	syscallarg(void *) a1;
	syscallarg(void *) a2;
	syscallarg(int) sel;
	syscallarg(void *) a3;
};

struct svr4_sys_execve_args {
	syscallarg(char *) path;
	syscallarg(char **) argp;
	syscallarg(char **) envp;
};

struct svr4_sys_fcntl_args {
	syscallarg(int) fd;
	syscallarg(int) cmd;
	syscallarg(char *) arg;
};

struct svr4_sys_ulimit_args {
	syscallarg(int) cmd;
	syscallarg(long) newlimit;
};

struct svr4_sys_getdents_args {
	syscallarg(int) fd;
	syscallarg(char *) buf;
	syscallarg(int) nbytes;
};

struct svr4_sys_getmsg_args {
	syscallarg(int) fd;
	syscallarg(struct svr4_strbuf *) ctl;
	syscallarg(struct svr4_strbuf *) dat;
	syscallarg(int *) flags;
};

struct svr4_sys_putmsg_args {
	syscallarg(int) fd;
	syscallarg(struct svr4_strbuf *) ctl;
	syscallarg(struct svr4_strbuf *) dat;
	syscallarg(int) flags;
};

struct svr4_sys_lstat_args {
	syscallarg(char *) path;
	syscallarg(struct svr4_stat *) ub;
};

struct svr4_sys_sigprocmask_args {
	syscallarg(int) how;
	syscallarg(svr4_sigset_t *) set;
	syscallarg(svr4_sigset_t *) oset;
};

struct svr4_sys_sigsuspend_args {
	syscallarg(svr4_sigset_t *) ss;
};

struct svr4_sys_sigaltstack_args {
	syscallarg(struct svr4_sigaltstack *) nss;
	syscallarg(struct svr4_sigaltstack *) oss;
};

struct svr4_sys_sigaction_args {
	syscallarg(int) signum;
	syscallarg(struct svr4_sigaction *) nsa;
	syscallarg(struct svr4_sigaction *) osa;
};

struct svr4_sys_sigpending_args {
	syscallarg(int) what;
	syscallarg(svr4_sigset_t *) mask;
};

struct svr4_sys_context_args {
	syscallarg(int) func;
	syscallarg(struct svr4_ucontext *) uc;
};

struct svr4_sys_statvfs_args {
	syscallarg(char *) path;
	syscallarg(struct svr4_statvfs *) fs;
};

struct svr4_sys_fstatvfs_args {
	syscallarg(int) fd;
	syscallarg(struct svr4_statvfs *) fs;
};

struct svr4_sys_waitsys_args {
	syscallarg(int) grp;
	syscallarg(int) id;
	syscallarg(union svr4_siginfo *) info;
	syscallarg(int) options;
};

struct svr4_sys_hrtsys_args {
	syscallarg(int) cmd;
	syscallarg(int) fun;
	syscallarg(int) sub;
	syscallarg(void *) rv1;
	syscallarg(void *) rv2;
};

struct svr4_sys_pathconf_args {
	syscallarg(char *) path;
	syscallarg(int) name;
};

struct svr4_sys_mmap_args {
	syscallarg(svr4_caddr_t) addr;
	syscallarg(svr4_size_t) len;
	syscallarg(int) prot;
	syscallarg(int) flags;
	syscallarg(int) fd;
	syscallarg(svr4_off_t) pos;
};

struct svr4_sys_fpathconf_args {
	syscallarg(int) fd;
	syscallarg(int) name;
};

struct svr4_sys_xstat_args {
	syscallarg(int) two;
	syscallarg(char *) path;
	syscallarg(struct svr4_xstat *) ub;
};

struct svr4_sys_lxstat_args {
	syscallarg(int) two;
	syscallarg(char *) path;
	syscallarg(struct svr4_xstat *) ub;
};

struct svr4_sys_fxstat_args {
	syscallarg(int) two;
	syscallarg(int) fd;
	syscallarg(struct svr4_xstat *) sb;
};

struct svr4_sys_xmknod_args {
	syscallarg(int) two;
	syscallarg(char *) path;
	syscallarg(svr4_mode_t) mode;
	syscallarg(svr4_dev_t) dev;
};

struct svr4_sys_setrlimit_args {
	syscallarg(int) which;
	syscallarg(struct ogetrlimit *) rlp;
};

struct svr4_sys_getrlimit_args {
	syscallarg(int) which;
	syscallarg(struct ogetrlimit *) rlp;
};

struct svr4_sys_memcntl_args {
	syscallarg(svr4_caddr_t) addr;
	syscallarg(svr4_size_t) len;
	syscallarg(int) cmd;
	syscallarg(svr4_caddr_t) arg;
	syscallarg(int) attr;
	syscallarg(int) mask;
};

struct svr4_sys_uname_args {
	syscallarg(struct svr4_utsname *) name;
	syscallarg(int) dummy;
};

struct svr4_sys_setegid_args {
	syscallarg(gid_t) egid;
};

struct svr4_sys_sysconfig_args {
	syscallarg(int) name;
};

struct svr4_sys_systeminfo_args {
	syscallarg(int) what;
	syscallarg(char *) buf;
	syscallarg(long) len;
};

struct svr4_sys_fchroot_args {
	syscallarg(int) fd;
};

struct svr4_sys_utimes_args {
	syscallarg(char *) path;
	syscallarg(struct timeval *) tptr;
};

struct svr4_sys_gettimeofday_args {
	syscallarg(struct timeval *) tp;
};

struct svr4_sys_pread_args {
	syscallarg(int) fd;
	syscallarg(void *) buf;
	syscallarg(size_t) nbyte;
	syscallarg(svr4_off_t) off;
};

struct svr4_sys_pwrite_args {
	syscallarg(int) fd;
	syscallarg(const void *) buf;
	syscallarg(size_t) nbyte;
	syscallarg(svr4_off_t) off;
};

struct svr4_sys_llseek_args {
	syscallarg(int) fd;
	syscallarg(long) offset1;
	syscallarg(long) offset2;
	syscallarg(int) whence;
};

struct svr4_sys_acl_args {
	syscallarg(char *) path;
	syscallarg(int) cmd;
	syscallarg(int) num;
	syscallarg(struct svr4_aclent *) buf;
};

struct svr4_sys_auditsys_args {
	syscallarg(int) code;
	syscallarg(int) a1;
	syscallarg(int) a2;
	syscallarg(int) a3;
	syscallarg(int) a4;
	syscallarg(int) a5;
};

struct svr4_sys_facl_args {
	syscallarg(int) fd;
	syscallarg(int) cmd;
	syscallarg(int) num;
	syscallarg(struct svr4_aclent *) buf;
};

struct svr4_sys_getdents64_args {
	syscallarg(int) fd;
	syscallarg(struct svr4_dirent64 *) dp;
	syscallarg(int) nbytes;
};

struct svr4_sys_mmap64_args {
	syscallarg(svr4_caddr_t) addr;
	syscallarg(svr4_size_t) len;
	syscallarg(int) prot;
	syscallarg(int) flags;
	syscallarg(int) fd;
	syscallarg(svr4_off64_t) pos;
};

struct svr4_sys_stat64_args {
	syscallarg(const char *) path;
	syscallarg(struct svr4_stat64 *) sb;
};

struct svr4_sys_lstat64_args {
	syscallarg(const char *) path;
	syscallarg(struct svr4_stat64 *) sb;
};

struct svr4_sys_fstat64_args {
	syscallarg(int) fd;
	syscallarg(struct svr4_stat64 *) sb;
};

struct svr4_sys_fstatvfs64_args {
	syscallarg(int) fd;
	syscallarg(struct svr4_statvfs64 *) fs;
};

struct svr4_sys_pread64_args {
	syscallarg(int) fd;
	syscallarg(void *) buf;
	syscallarg(size_t) nbyte;
	syscallarg(svr4_off64_t) off;
};

struct svr4_sys_pwrite64_args {
	syscallarg(int) fd;
	syscallarg(const void *) buf;
	syscallarg(size_t) nbyte;
	syscallarg(svr4_off64_t) off;
};

struct svr4_sys_creat64_args {
	syscallarg(char *) path;
	syscallarg(int) mode;
};

struct svr4_sys_open64_args {
	syscallarg(char *) path;
	syscallarg(int) flags;
	syscallarg(int) mode;
};

struct svr4_sys_socket_args {
	syscallarg(int) domain;
	syscallarg(int) type;
	syscallarg(int) protocol;
};

/*
 * System call prototypes.
 */

int	sys_nosys	__P((struct proc *, void *, register_t *));
int	sys_exit	__P((struct proc *, void *, register_t *));
int	sys_fork	__P((struct proc *, void *, register_t *));
int	sys_read	__P((struct proc *, void *, register_t *));
int	sys_write	__P((struct proc *, void *, register_t *));
int	svr4_sys_open	__P((struct proc *, void *, register_t *));
int	sys_close	__P((struct proc *, void *, register_t *));
int	svr4_sys_wait	__P((struct proc *, void *, register_t *));
int	svr4_sys_creat	__P((struct proc *, void *, register_t *));
int	sys_link	__P((struct proc *, void *, register_t *));
int	sys_unlink	__P((struct proc *, void *, register_t *));
int	svr4_sys_execv	__P((struct proc *, void *, register_t *));
int	sys_chdir	__P((struct proc *, void *, register_t *));
int	svr4_sys_time	__P((struct proc *, void *, register_t *));
int	svr4_sys_mknod	__P((struct proc *, void *, register_t *));
int	sys_chmod	__P((struct proc *, void *, register_t *));
int	sys_chown	__P((struct proc *, void *, register_t *));
int	svr4_sys_break	__P((struct proc *, void *, register_t *));
int	svr4_sys_stat	__P((struct proc *, void *, register_t *));
int	compat_43_sys_lseek	__P((struct proc *, void *, register_t *));
int	sys_getpid	__P((struct proc *, void *, register_t *));
int	sys_setuid	__P((struct proc *, void *, register_t *));
int	sys_getuid	__P((struct proc *, void *, register_t *));
int	svr4_sys_alarm	__P((struct proc *, void *, register_t *));
int	svr4_sys_fstat	__P((struct proc *, void *, register_t *));
int	svr4_sys_pause	__P((struct proc *, void *, register_t *));
int	svr4_sys_utime	__P((struct proc *, void *, register_t *));
int	svr4_sys_access	__P((struct proc *, void *, register_t *));
int	svr4_sys_nice	__P((struct proc *, void *, register_t *));
int	sys_sync	__P((struct proc *, void *, register_t *));
int	svr4_sys_kill	__P((struct proc *, void *, register_t *));
int	svr4_sys_pgrpsys	__P((struct proc *, void *, register_t *));
int	sys_dup	__P((struct proc *, void *, register_t *));
int	sys_opipe	__P((struct proc *, void *, register_t *));
int	svr4_sys_times	__P((struct proc *, void *, register_t *));
int	sys_setgid	__P((struct proc *, void *, register_t *));
int	sys_getgid	__P((struct proc *, void *, register_t *));
int	svr4_sys_signal	__P((struct proc *, void *, register_t *));
#ifdef SYSVMSG
int	svr4_sys_msgsys	__P((struct proc *, void *, register_t *));
#else
#endif
int	svr4_sys_sysarch	__P((struct proc *, void *, register_t *));
#ifdef SYSVSHM
int	svr4_sys_shmsys	__P((struct proc *, void *, register_t *));
#else
#endif
#ifdef SYSVSEM
int	svr4_sys_semsys	__P((struct proc *, void *, register_t *));
#else
#endif
int	svr4_sys_ioctl	__P((struct proc *, void *, register_t *));
int	svr4_sys_utssys	__P((struct proc *, void *, register_t *));
int	sys_fsync	__P((struct proc *, void *, register_t *));
int	svr4_sys_execve	__P((struct proc *, void *, register_t *));
int	sys_umask	__P((struct proc *, void *, register_t *));
int	sys_chroot	__P((struct proc *, void *, register_t *));
int	svr4_sys_fcntl	__P((struct proc *, void *, register_t *));
int	svr4_sys_ulimit	__P((struct proc *, void *, register_t *));
int	svr4_sys_rdebug	__P((struct proc *, void *, register_t *));
int	sys_rmdir	__P((struct proc *, void *, register_t *));
int	sys_mkdir	__P((struct proc *, void *, register_t *));
int	svr4_sys_getdents	__P((struct proc *, void *, register_t *));
int	svr4_sys_getmsg	__P((struct proc *, void *, register_t *));
int	svr4_sys_putmsg	__P((struct proc *, void *, register_t *));
int	sys_poll	__P((struct proc *, void *, register_t *));
int	svr4_sys_lstat	__P((struct proc *, void *, register_t *));
int	sys_symlink	__P((struct proc *, void *, register_t *));
int	sys_readlink	__P((struct proc *, void *, register_t *));
int	sys_getgroups	__P((struct proc *, void *, register_t *));
int	sys_setgroups	__P((struct proc *, void *, register_t *));
int	sys_fchmod	__P((struct proc *, void *, register_t *));
int	sys_fchown	__P((struct proc *, void *, register_t *));
int	svr4_sys_sigprocmask	__P((struct proc *, void *, register_t *));
int	svr4_sys_sigsuspend	__P((struct proc *, void *, register_t *));
int	svr4_sys_sigaltstack	__P((struct proc *, void *, register_t *));
int	svr4_sys_sigaction	__P((struct proc *, void *, register_t *));
int	svr4_sys_sigpending	__P((struct proc *, void *, register_t *));
int	svr4_sys_context	__P((struct proc *, void *, register_t *));
int	svr4_sys_statvfs	__P((struct proc *, void *, register_t *));
int	svr4_sys_fstatvfs	__P((struct proc *, void *, register_t *));
int	svr4_sys_waitsys	__P((struct proc *, void *, register_t *));
int	svr4_sys_hrtsys	__P((struct proc *, void *, register_t *));
int	svr4_sys_pathconf	__P((struct proc *, void *, register_t *));
int	sys_mincore	__P((struct proc *, void *, register_t *));
int	svr4_sys_mmap	__P((struct proc *, void *, register_t *));
int	sys_mprotect	__P((struct proc *, void *, register_t *));
int	sys_munmap	__P((struct proc *, void *, register_t *));
int	svr4_sys_fpathconf	__P((struct proc *, void *, register_t *));
int	sys_vfork	__P((struct proc *, void *, register_t *));
int	sys_fchdir	__P((struct proc *, void *, register_t *));
int	sys_readv	__P((struct proc *, void *, register_t *));
int	sys_writev	__P((struct proc *, void *, register_t *));
int	svr4_sys_xstat	__P((struct proc *, void *, register_t *));
int	svr4_sys_lxstat	__P((struct proc *, void *, register_t *));
int	svr4_sys_fxstat	__P((struct proc *, void *, register_t *));
int	svr4_sys_xmknod	__P((struct proc *, void *, register_t *));
int	svr4_sys_setrlimit	__P((struct proc *, void *, register_t *));
int	svr4_sys_getrlimit	__P((struct proc *, void *, register_t *));
int	sys_lchown	__P((struct proc *, void *, register_t *));
int	svr4_sys_memcntl	__P((struct proc *, void *, register_t *));
int	sys_rename	__P((struct proc *, void *, register_t *));
int	svr4_sys_uname	__P((struct proc *, void *, register_t *));
int	svr4_sys_setegid	__P((struct proc *, void *, register_t *));
int	svr4_sys_sysconfig	__P((struct proc *, void *, register_t *));
int	sys_adjtime	__P((struct proc *, void *, register_t *));
int	svr4_sys_systeminfo	__P((struct proc *, void *, register_t *));
int	sys_seteuid	__P((struct proc *, void *, register_t *));
int	svr4_sys_fchroot	__P((struct proc *, void *, register_t *));
int	svr4_sys_utimes	__P((struct proc *, void *, register_t *));
int	svr4_sys_vhangup	__P((struct proc *, void *, register_t *));
int	svr4_sys_gettimeofday	__P((struct proc *, void *, register_t *));
int	sys_getitimer	__P((struct proc *, void *, register_t *));
int	sys_setitimer	__P((struct proc *, void *, register_t *));
int	svr4_sys_pread	__P((struct proc *, void *, register_t *));
int	svr4_sys_pwrite	__P((struct proc *, void *, register_t *));
int	svr4_sys_llseek	__P((struct proc *, void *, register_t *));
int	svr4_sys_acl	__P((struct proc *, void *, register_t *));
int	svr4_sys_auditsys	__P((struct proc *, void *, register_t *));
int	sys_clock_gettime	__P((struct proc *, void *, register_t *));
int	sys_clock_settime	__P((struct proc *, void *, register_t *));
int	sys_clock_getres	__P((struct proc *, void *, register_t *));
int	sys_nanosleep	__P((struct proc *, void *, register_t *));
int	svr4_sys_facl	__P((struct proc *, void *, register_t *));
int	compat_43_sys_setreuid	__P((struct proc *, void *, register_t *));
int	compat_43_sys_setregid	__P((struct proc *, void *, register_t *));
int	svr4_sys_getdents64	__P((struct proc *, void *, register_t *));
int	svr4_sys_mmap64	__P((struct proc *, void *, register_t *));
int	svr4_sys_stat64	__P((struct proc *, void *, register_t *));
int	svr4_sys_lstat64	__P((struct proc *, void *, register_t *));
int	svr4_sys_fstat64	__P((struct proc *, void *, register_t *));
int	svr4_sys_fstatvfs64	__P((struct proc *, void *, register_t *));
int	svr4_sys_pread64	__P((struct proc *, void *, register_t *));
int	svr4_sys_pwrite64	__P((struct proc *, void *, register_t *));
int	svr4_sys_creat64	__P((struct proc *, void *, register_t *));
int	svr4_sys_open64	__P((struct proc *, void *, register_t *));
int	svr4_sys_socket	__P((struct proc *, void *, register_t *));
int	sys_socketpair	__P((struct proc *, void *, register_t *));
int	sys_bind	__P((struct proc *, void *, register_t *));
int	sys_listen	__P((struct proc *, void *, register_t *));
int	compat_43_sys_accept	__P((struct proc *, void *, register_t *));
int	sys_connect	__P((struct proc *, void *, register_t *));
int	sys_shutdown	__P((struct proc *, void *, register_t *));
int	compat_43_sys_recv	__P((struct proc *, void *, register_t *));
int	compat_43_sys_recvfrom	__P((struct proc *, void *, register_t *));
int	compat_43_sys_recvmsg	__P((struct proc *, void *, register_t *));
int	compat_43_sys_send	__P((struct proc *, void *, register_t *));
int	compat_43_sys_sendmsg	__P((struct proc *, void *, register_t *));
int	sys_sendto	__P((struct proc *, void *, register_t *));
int	compat_43_sys_getpeername	__P((struct proc *, void *, register_t *));
int	compat_43_sys_getsockname	__P((struct proc *, void *, register_t *));
int	sys_getsockopt	__P((struct proc *, void *, register_t *));
int	sys_setsockopt	__P((struct proc *, void *, register_t *));
#ifdef NTP
int	sys_ntp_gettime	__P((struct proc *, void *, register_t *));
int	sys_ntp_adjtime	__P((struct proc *, void *, register_t *));
#else
#endif
