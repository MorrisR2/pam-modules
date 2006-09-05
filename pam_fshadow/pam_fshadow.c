/* This file is part of pam-modules.
 * Copyright (C) 2001,2005 Sergey Poznyakoff
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA  
 */

/* pam_fshadow */
 
#include <security/_pam_aconf.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pwd.h>
#include <shadow.h>

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT

#include <security/pam_modules.h>

#include <common.c>

/* some syslogging */

static void
_pam_log(int err, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vsyslog(err|LOG_AUTH, format, args);
	va_end(args);
}

static void
_pam_debug(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vsyslog(LOG_AUTH|LOG_DEBUG, format, args);
	va_end(args);
}

char *sysconfdir = SYSCONFDIR;
static int cntl_flags = 0;

#define CNTL_DEBUG       0x0001
#define CNTL_AUTHTOK     0x0002
#define CNTL_NOPASSWD    0x0004

#define CNTL_DEBUG_LEV() (cntl_flags>>16)
#define CNTL_SET_DEBUG_LEV(cntl,n) (cntl |= ((n)<<16))
#define DEBUG(m,c) if (CNTL_DEBUG_LEV()>=(m)) _pam_debug c

static int
_pam_parse(int argc, const char **argv)
{
	/* step through arguments */
	for (cntl_flags = 0; argc-- > 0; ++argv) {

		/* generic options */
		
		if (!strncmp(*argv,"debug",5)) {
			cntl_flags |= CNTL_DEBUG;
			if ((*argv)[5] == '=') 
				CNTL_SET_DEBUG_LEV(cntl_flags,atoi(*argv+6));
			else
				CNTL_SET_DEBUG_LEV(cntl_flags,1);
		} else if (!strcmp(*argv,"use_authtok"))
			cntl_flags |= CNTL_AUTHTOK;
		else if (!strncmp(*argv, "sysconfdir=",11))
			sysconfdir = (char*) (*argv+11);
		else if (!strcmp(*argv, "nopasswd"))
			cntl_flags |= CNTL_NOPASSWD;
		else 
			_pam_log(LOG_ERR,
				 "pam_parse: unknown option; %s", *argv);
	}
	
	return cntl_flags;
}

static int
converse(pam_handle_t *pamh,
	 int nargs,
	 struct pam_message **message,
	 struct pam_response **response)
{
	int retval;
	struct pam_conv *conv;

	DEBUG(100,("enter converse"));

	retval = pam_get_item(pamh, PAM_CONV, (const void **) &conv);
	DEBUG(10,("pam_get_item(PAM_CONV): %d", retval));
	if (retval == PAM_SUCCESS) {

		retval = conv->conv(nargs,
				    (const struct pam_message **) message,
				    response,
				    conv->appdata_ptr);
		
		DEBUG(10, ("app conversation returned %d", retval));

		if (retval != PAM_SUCCESS) {
			_pam_log(LOG_ERR,
				 "conversation failure [%s]",
				 pam_strerror(pamh, retval));
		}
	} else if (retval != PAM_CONV_AGAIN) {
		_pam_log(LOG_ERR, 
		         "couldn't obtain coversation function: %s",
			 pam_strerror(pamh, retval));
	}

	DEBUG(100,("exit converse: %d", retval));

	return retval;		/* propagate error status */
}

static int
_pam_get_password(pam_handle_t *pamh, char **password, const char *prompt)
{
	char *item, *token;
	int retval;
	struct pam_message msg[3], *pmsg[3];
	struct pam_response *resp;
	int i, replies;

	DEBUG(100,("enter _pam_get_password"));
	
	if (cntl_flags & CNTL_AUTHTOK) {
		/*
		 * get the password from the PAM item
		 */
		retval = pam_get_item(pamh, PAM_AUTHTOK,
				      (const void **) &item);
		if (retval != PAM_SUCCESS) {
			/* very strange. */
			_pam_log(LOG_ALERT,
				 "can't retrieve password item: %s",
				 pam_strerror(pamh, retval));
			return retval;
		} else if (item != NULL) {
			*password = item;
			item = NULL;
			return PAM_SUCCESS;
		} else
			return PAM_AUTHTOK_RECOVER_ERR;
	}

	/*
	 * ask user for the password
	 */
	/* prepare to converse */

	i = 0;
	pmsg[i] = &msg[i];
	msg[i].msg_style = PAM_PROMPT_ECHO_OFF;
	msg[i++].msg = (const void*)prompt;
	replies = 1;

	/* run conversation */
	resp = NULL;
	token = NULL;
	retval = converse(pamh, i, pmsg, &resp);

	if (resp != NULL) {
		if (retval == PAM_SUCCESS) { 	/* a good conversation */
			token = XSTRDUP(resp[i - replies].resp);
			DEBUG(10,("app returned [%s]", token));
			PAM_DROP_REPLY(resp, 1);
		} else {
			_pam_log(LOG_ERR, "conversation error: %s",
				 pam_strerror(pamh, retval));
		}
		
	} else {
		retval = (retval == PAM_SUCCESS)
			? PAM_AUTHTOK_RECOVER_ERR : retval;
	}

	if (retval == PAM_SUCCESS) {
		/*
		 * keep password as data specific to this module. pam_end()
		 * will arrange to clean it up.
		 */
		retval = pam_set_data(pamh, "password",
				      (void *)token,
				      _cleanup_string);
		if (retval != PAM_SUCCESS) {
			_pam_log(LOG_CRIT, 
			         "can't keep password: %s",
				 pam_strerror(pamh, retval));
			_pam_delete(token);
		} else {
			*password = token;
			token = NULL;	/* break link to password */
		}
	} else {
		_pam_log(LOG_ERR,
			 "unable to obtain a password: %s",
			 pam_strerror(pamh, retval));
	} 
	
	DEBUG(100,("exit _pam_get_password: %d", retval));
	return retval;
}

char *
mkfilename(char *dir, char *name)
{
        int len = strlen(dir) + strlen(name);
        char *p = malloc(len+2);
	if (!p) {
		_pam_log(LOG_EMERG, "not enough memory");
		abort ();
	}
        sprintf(p, "%s/%s", dir, name);
        return p;
}

int
verify_user_acct(const char *username)
{
	char *filename = mkfilename(sysconfdir, "passwd");
	FILE *fp;
	int retval;
	
	fp = fopen (filename, "r");
	if (fp) {
		struct passwd *pw;

		while ((pw = fgetpwent (fp)) != NULL) {
			if (strcmp (pw->pw_name, username) == 0)
				break;
		}
		if (!pw) {
			_pam_log(LOG_ERR, "user %s not found in %s",
				 username, filename);
			retval = PAM_USER_UNKNOWN;
		} else
			retval = PAM_SUCCESS;
	} else {
		_pam_log(LOG_ERR, "can't open %s: %s",
			 filename, strerror(errno));
		retval = PAM_SERVICE_ERR;
	}
	free(filename);
	return retval;
}

int
verify_user_pass(const char *username, const char *password)
{
	struct spwd *sp = NULL;
	time_t curdays;
	FILE *fp;
	int retval = PAM_AUTH_ERR;
	char *shadow = mkfilename(sysconfdir, "shadow");
		
	DEBUG(10,("Verify user `%s' with password `%s'",
		  username, password));

	fp = fopen(shadow, "r");
	if (!fp) {
		_pam_log(LOG_ERR,
			 "can't open %s: %s", shadow, strerror(errno));
		free(shadow);
		return PAM_SERVICE_ERR;
	}

	while ((sp = fgetspent(fp)) != NULL
	       && strcmp(sp->sp_namp, username))
		;
	fclose(fp);
	
	if (!sp) {
		_pam_log(LOG_ERR,
			 "entry for %s not found in %s",
			 username, shadow);
		free(shadow);
		return PAM_USER_UNKNOWN;
	}

	/* We have the user's information, now let's check if his account
	   has expired */
	curdays = time(NULL) / (60 * 60 * 24);
	if (sp->sp_min != -1 && curdays < sp->sp_lstchg + sp->sp_min)
		retval = PAM_AUTHTOK_ERR;
	else if (sp->sp_max != -1 && sp->sp_inact != -1 && sp->sp_lstchg != 0
		 && curdays > sp->sp_lstchg + sp->sp_max + sp->sp_inact)
		/* Password is too old */
		retval = PAM_ACCT_EXPIRED;
	else if (sp->sp_expire != -1 && sp->sp_lstchg != 0
		 && curdays > sp->sp_expire)
		/* Account has expired */
		retval = PAM_ACCT_EXPIRED;
	else if (strcmp(sp->sp_pwdp, crypt(password, sp->sp_pwdp)) == 0)
		retval = PAM_SUCCESS;
	else
		retval = PAM_AUTH_ERR;

	free(shadow);
	return retval;
}

/* --- authentication management functions (only) --- */

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags,
		    int argc, const char **argv)
{
	const char *username;
	char *password;
	int retval = PAM_AUTH_ERR;

	/* parse arguments */
	_pam_parse(argc, argv);

	/* Get the username */
	retval = pam_get_user(pamh, &username, NULL);
	if (retval != PAM_SUCCESS || !username) {
		_pam_log(LOG_DEBUG,"can not get the username");
		return PAM_SERVICE_ERR;
	}

	/* Get the password */
	if (_pam_get_password(pamh, &password, "Password:"))
		return PAM_SERVICE_ERR;

	if (retval != PAM_SUCCESS) {
		_pam_log(LOG_ERR, "Could not retrive user's password");
		return -2;
	}

	if (cntl_flags & CNTL_NOPASSWD)
		retval = 0;
	else
		retval = verify_user_acct(username);
	if (retval == PAM_SUCCESS)
		retval = verify_user_pass(username, password);
	
	switch (retval) {
	case PAM_ACCT_EXPIRED:
		_pam_log(LOG_NOTICE, "user '%s': account expired", username);
		break;
	case PAM_SUCCESS:
		_pam_log(LOG_NOTICE, "user '%s' granted access", username);
		break;
	default:
		_pam_log(LOG_NOTICE, "user '%s' failed to authenticate",
			 username);
	}

	return retval;
}

PAM_EXTERN
int pam_sm_setcred(pam_handle_t *pamh, int flags,
		   int argc, const char **argv)
{
    return PAM_SUCCESS;
}

PAM_EXTERN
int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags,
		   int argc, const char **argv)
{
    return PAM_SUCCESS;
}


#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_fshadow_modstruct = {
     "pam_fshadow",
     pam_sm_authenticate,
     pam_sm_setcred,
     NULL,
     NULL,
     NULL,
     NULL,
};

#endif

