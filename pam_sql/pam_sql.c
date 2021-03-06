/* This file is part of pam-modules.
   Copyright (C) 2005-2008, 2010-2012, 2014-2015 Sergey Poznyakoff

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3 of the License, or (at your
   option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include "pam_sql.h"

/* indicate the following groups are defined */
#define PAM_SM_AUTH
#define PAM_SM_SESSION

#define CNTL_AUTHTOK      0x0010

static int cntl_flags;
long gpam_sql_debug_level;
char *gpam_sql_config_file = SYSCONFDIR "/pam_sql.conf";

struct pam_opt pam_opt[] = {
	{ PAM_OPTSTR(debug), pam_opt_long, &debug_level },
	{ PAM_OPTSTR(debug), pam_opt_const, &debug_level, { 1 } },
	{ PAM_OPTSTR(audit), pam_opt_const, &debug_level, { 100 } },
	{ PAM_OPTSTR(waitdebug), pam_opt_null, NULL, { 0 },
	  gray_wait_debug_fun },
	{ PAM_OPTSTR(use_authtok), pam_opt_bitmask, &cntl_flags,
	  { CNTL_AUTHTOK } },
	{ PAM_OPTSTR(config), pam_opt_string, &gpam_sql_config_file },
	{ NULL }
};

static void
_pam_parse(int argc, const char **argv)
{
	cntl_flags = 0;
	debug_level = 0;
	gpam_sql_config_file = SYSCONFDIR "/pam_sql.conf";
	gray_log_init(0, gpam_sql_module_name, LOG_AUTHPRIV);
	gray_parseopt(pam_opt, argc, argv);
}


static int
_pam_get_password(pam_handle_t *pamh, char **password, const char *prompt)
{
	char *item, *token;
	int retval;
	struct pam_message msg[3], *pmsg[3];
	struct pam_response *resp;
	int i, replies;

	DEBUG(90,("enter _pam_get_password"));
	
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
	retval = gray_converse(pamh, i, pmsg, &resp);

	if (resp != NULL && resp[i - replies].resp) {
		if (retval == PAM_SUCCESS) { 	/* a good conversation */
			token = XSTRDUP(resp[i - replies].resp);
			DEBUG(100,("app returned [%s]", token));
			pam_set_item(pamh, PAM_AUTHTOK, token);
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
				      gray_cleanup_string);
		if (retval != PAM_SUCCESS) {
			_pam_log(LOG_CRIT, 
			         "can't keep password: %s",
				 pam_strerror(pamh, retval));
			gray_pam_delete(token);
		} else {
			*password = token;
			token = NULL;	/* break link to password */
		}
	} else {
		_pam_log(LOG_ERR,
			 "unable to obtain a password: %s",
			 pam_strerror(pamh, retval));
	} 
	
	DEBUG(90,("exit _pam_get_password: %d", retval));
	return retval;
}


/* Configuration */
static struct gray_env *config_env;

char *
gpam_sql_find_config(const char *name)
{
	return gray_env_get(config_env, name);
}

int
gpam_sql_check_boolean_config(const char *name, int defval)
{
	const char *value = gpam_sql_find_config(name);
	if (value)
		defval = gray_boolean_true_p(value);
	return defval;
}


const char *
gpam_sql_get_query(pam_handle_t *pamh, const char *name, gray_slist_t *pslist,
		   int required)
{
	gray_slist_t slist;
	const char *query = gpam_sql_find_config(name);

 	if (!query) {
		if (required)
			gray_raise("%s: %s not defined", gpam_sql_config_file, name);
		return NULL;
	}
	
	slist = gray_slist_create();
	gray_expand_string(pamh, query, slist);
	gray_slist_append_char(slist, 0);
	*pslist = slist;
	return gray_slist_finish(slist);
}

static const char *
get_query2(pam_handle_t *pamh, const char *name1, const char *name2,
	   gray_slist_t *pslist, int required)
{
	gray_slist_t slist;
	const char *query = gpam_sql_find_config(name1);

	if (!query)
		query = gpam_sql_find_config(name2);
	
 	if (!query) {
		if (required)
			gray_raise("%s: %s not defined", 
			           gpam_sql_config_file, name1);
		return NULL;
	}
	
	slist = gray_slist_create();
	gray_expand_string(pamh, query, slist);
	gray_slist_append_char(slist, 0);
	*pslist = slist;
	return gray_slist_finish(slist);
}


/* --- authentication management functions (only) --- */

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	const char *username;
	char *password;
	int retval = PAM_AUTH_ERR;
	gray_pam_init(PAM_SERVICE_ERR);

	/* parse arguments */
	_pam_parse(argc, argv);
	
	/* Get the username */
	retval = pam_get_user(pamh, &username, NULL);
	if (retval != PAM_SUCCESS || !username) {
		DEBUG(1, ("can not get the username"));
		return PAM_SERVICE_ERR;
	}

	/* Get the password */
	if (_pam_get_password(pamh, &password, "Password:"))
		return PAM_SERVICE_ERR;

	if (retval != PAM_SUCCESS) {
		_pam_log(LOG_ERR, "Could not retrive user's password");
		return PAM_SERVICE_ERR;
	}

	if (gray_env_read(gpam_sql_config_file, &config_env)) 
		retval = PAM_SERVICE_ERR;
	else {
		gray_slist_t slist;
		/* FIXME: This comment is needed to pacify
		   `make check-sql-config' in doc:
		   gpam_sql_find_config("passwd-query") */
		retval = gpam_sql_verify_user_pass(pamh, password,
					     get_query2(pamh, "passwd-query",
					     "query",  &slist, 1));
		gray_slist_free(&slist);
	}
	
	gray_env_free(config_env);
	config_env = NULL;
	
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

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}

static int
sql_session_mgmt(pam_handle_t *pamh, int flags,
		 int argc, const char **argv, const char *query_name)
{
	int retval;

	gray_pam_init(PAM_SERVICE_ERR);

	/* parse arguments */
	_pam_parse(argc, argv);

	if (gray_env_read(gpam_sql_config_file, &config_env)) 
		retval = PAM_SERVICE_ERR;
	else {
		gray_slist_t slist;
		retval = gpam_sql_acct(pamh,
				       gpam_sql_get_query(pamh, query_name,
							  &slist, 0));
		gray_slist_free(&slist);
	}
	
	gray_env_free(config_env);
	config_env = NULL;
	
	return retval;
}

PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	/* FIXME: This comment is needed to pacify `make check-sql-config'
	   in doc:
	   gpam_sql_find_config("session-start-query") */
	return sql_session_mgmt(pamh, flags, argc, argv,
				"session-start-query");
}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh, int flags,
		     int argc, const char **argv)
{
	/* FIXME: This comment is needed to pacify `make check-sql-config'
	   in doc:
	   gpam_sql_find_config("session-stop-query") */
	return sql_session_mgmt(pamh, flags, argc, argv,
				"session-stop-query");
}
