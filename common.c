/* This file is part of pam-modules.
   Copyright (C) 2001 Sergey Poznyakoff
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301 USA  */

#ifndef PAM_CONV_AGAIN
# define PAM_CONV_AGAIN PAM_TRY_AGAIN
#endif
#ifndef PAM_AUTHTOK_RECOVER_ERR
# define PAM_AUTHTOK_RECOVER_ERR PAM_AUTHTOK_RECOVERY_ERR
#endif
#ifndef PAM_EXTERN
# define PAM_EXTERN
#endif

#define XSTRDUP(s) (s) ? strdup(s) : NULL

#define PAM_OVERWRITE(s)                              \
  do {                                                \
	register char *p;                             \
        if  ((p = s) != NULL)                         \
	    while (*p) *p++ = 0;                      \
  } while (0) 

#define PAM_DROP_REPLY(reply, nrepl)                  \
  do {                                                \
	int i;                                        \
	for (i=0; i<nrepl; i++) {                     \
            PAM_OVERWRITE(reply[i].resp);             \
            free(reply[i].resp);                      \
	}                                             \
	if (reply)                                    \
	    free(reply);                              \
  } while (0)
	
static void
_pam_delete(char *x)
{
	PAM_OVERWRITE(x);
	free(x);
}

static void
_cleanup_string(pam_handle_t *pamh, void *x, int error_status)
{
	_pam_delete(x);
}

/* Syslog functions */
static int syslog_dont_open = 0;
static const char *syslog_tag = MODULE_NAME;
static int facility = LOG_AUTHPRIV;

static void
_pam_vlog(int err, const char *format, va_list args)
{
	if (syslog_dont_open)
		err |= facility;
	else
		openlog(syslog_tag, LOG_CONS|LOG_PID, facility);
	vsyslog(err, format, args);
	if (!syslog_dont_open)
		closelog();
}

static void
_pam_log(int err, const char *format, ...)
{
	va_list args;
	
	va_start(args, format);
	_pam_vlog(err, format, args);
	va_end(args);
}

static void
_pam_debug(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	_pam_vlog(LOG_DEBUG, format, args);
	va_end(args);
}

void
wait_debug(size_t interval, const char *file, size_t line)
{
#ifdef DEBUG_MODE
	if (!interval)
		interval = 3600;
	_pam_log(LOG_CRIT, "WAITING FOR DEBUG AT %s:%d",
		 file, (unsigned long)line);
	while (interval-- > 0)
		sleep();
#else
	_pam_log(LOG_NOTICE, "Debugging is not configured");
#endif	
}

#define WAITDEBUG(arg) do { size_t line = __LINE__;       \
  if ((arg)[0] == '=')                                    \
     wait_debug(atoi((arg)+1), __FILE__, line);           \
  else                                                    \
     wait_debug(0, __FILE__, line);		          \
} while (0)
