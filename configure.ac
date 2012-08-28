# Autoconf file for Comfirm DMARC

AC_INIT([comfirm-dmarc], [1.0])

AM_INIT_AUTOMAKE([1.10 -Wall no-define])

AC_CONFIG_HEADERS([src/config.h])

# Checks for programs
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

# Checks for libraries
AC_CHECK_LIB(pthread, sem_init, hiredis)

# Checks for header files
AC_CHECK_HEADERS(stdio.h unistd.h stdlib.h fcntl.h pthread.h sys/epoll.h sys/event.h signal.h netinet/tcp.h sched.h hiredis.h firm-dkim.h)

# Create Makefile
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
