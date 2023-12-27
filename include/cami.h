/*
 * CAMI -- C Asterisk Manager Interface
 *
 * Copyright (C) 2022, Naveen Albert
 *
 * Naveen Albert <asterisk@phreaknet.org>
 *
 * This program is free software, distributed under the terms of
 * the Mozilla Public License Version 2.


 * \file
 *
 * \brief C Asterisk Manager Interface
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 */

#define CAMI_VERSION_MAJOR 0
#define CAMI_VERSION_MINOR 2
#define CAMI_VERSION_PATCH 0

/* Max wait time in ms. Don't be tempted to make this too big, as this blocks all AMI traffic. Most of the time, it shouldn't really matter though. */
#define AMI_MAX_WAIT_TIME 1000

/* Currently, it is expected that any single AMI response fit within a single buffer, so responses larger than this may be truncated and corrupted. */
#define AMI_BUFFER_SIZE 1048576

struct ami_session;

struct ami_field {
	char *key;		/*!< AMI field key */
	char *value;	/*!< AMI field value */
};

struct ami_event {
	int size;		/*!< Number of fields in event */
	int actionid;	/*!< Action ID (internal) */
	struct ami_session *ami; /*!< Session that owns this event */
	struct ami_field fields[];	/*!< Fields */
};

struct ami_response {
	int success:1;	/*!< Response indicates success? */
	int size;		/*!< Number of events, including the "event" at index 0 containing the fields for the response itself */
	int actionid;	/*!< ActionID for response */
	/* Sadly we cannot have something like struct ami_field fields[] here, because you can only have one flexible array member */
	struct ami_event *events[];	/*!< Events */
};

/*!
 * \brief Enable debug logging
 * \param ami
 * \param fd File descriptor to which optional debug log messages should be delivered. Default is off (-1)
 * \note This is not recommended for use in production, but may be helpful in a dev environment.
 */
void ami_set_debug(struct ami_session *ami, int fd);

/*!
 * \brief Set debug logging level
 * \param level Level between 0 and 10. 0 will disable logging, 10 is the most granular. Default is 0.
 * \note A log level of 1 is recommended for production use: this will log all errors and warnings. Use a greater log level for debugging.
 * \retval -1 on failure, non-negative old log level otherwise
 */
int ami_set_debug_level(struct ami_session *ami, int level);

/*!
 * \brief Initialize an AMI connection with Asterisk
 * \param hostname Hostname (use 127.0.0.1 for localhost)
 * \param port Port number. Use 0 for the default port (5038)
 * \param callback Callback function for AMI events (not including responses to actions).
 * \param dis_callback Callback function if Asterisk disconnects our AMI connection. NOT invoked when ami_disconnect is called. This function is blocking so don't do anything too crazy inside.
 * \return NULL on failure
 * \return AMI session on success
 */
struct ami_session *ami_connect(const char *hostname, int port, void (*callback)(struct ami_session *ami, struct ami_event *event), void (*dis_callback)(struct ami_session *ami));

/*!
 * \brief Close an existing AMI connection
 * \param ami
 */
int ami_disconnect(struct ami_session *ami);

/*!
 * \brief Completely destroy and free an AMI session
 * \param ami
 * \note ami_disconnect should be called prior to this
 */
void ami_destroy(struct ami_session *ami);

/*!
 * \brief Print out the contents of an ami_event to stderr
 * \param event An AMI event
 */
void ami_dump_event(struct ami_event *event);

/*!
 * \brief Print out the contents of an ami_response to stderr
 * \param resp An AMI response
 */
void ami_dump_response(struct ami_response *resp);

/*!
 * \brief Retrieve the value of a specified key in an AMI event
 * \param event An AMI event
 * \brief key The name of the key of interest
 * \retval Key value if found or NULL if not found
 * \note You should strdup the return value if needed beyond the lifetime of event, or if you are going to modify it.
 */
const char *ami_keyvalue(struct ami_event *event, const char *key);

/*!
 * \brief Free an AMI event
 * \param event AMI event
 * \note You must use this to free an AMI event! Do not use free, or you will create a memory leak!
 */
void ami_event_free(struct ami_event *event);

/*!
 * \brief Free an AMI response
 * \param resp AMI response
 * \note You must use this to free an AMI response! Do not use free, or you will create a memory leak!
 * \note This function will automatically free any events encapsulated in it (no need to call ami_event_free for responses)
 */
void ami_resp_free(struct ami_response *resp);

/*!
 * \brief Log in to an AMI session
 * \param ami
 * \param username Asterisk AMI user username
 * \param password Asterisk AMI user secret
 * \retval 0 on success, -1 on failure
 * \note Assuming ami_connect was successful, this should be the first thing you call before doing anything else.
 */
int ami_action_login(struct ami_session *ami, const char *username, const char *password);

/*!
 * \brief Try to determine the AMI password from manager.conf, if we have access to it
 * \note This is a convenience function and will only work in the most simplistic cases (same host, user with read access to /etc/asterisk/manager.conf)
 */
int ami_auto_detect_ami_pass(const char *amiusername, char *buf, size_t buflen);

/*!
 * \brief Request a custom AMI action
 * \param ami
 * \param action Name of AMI action (as defined by Asterisk)
 * \param fmt Format string containing any action-specified AMI parameters, followed by your arguments (just like printf). Do NOT end with newlines.
 * \note Do NOT include any kind of ActionID. This is handled internally.
 */
struct ami_response *ami_action(struct ami_session *ami, const char *action, const char *fmt, ...);

/*!
 * \brief Set if failure responses should automatically be discarded
 * \param ami
 * \param discard 1 to discard failure responses and return NULL, 0 to return the raw failure response
 */
void ami_set_discard_on_failure(struct ami_session *ami, int discard);

/*!
 * \brief See if an action was successful and discard the response. Useful if you only care if an action succeeded and don't need the raw response (typically for "set", not "get" operations).
 * \param ami
 * \param resp AMI response
 * \retval 0 if the response indicated success, -1 if it indicated failure
 * \note This frees resp so resp will no longer be a valid pointer after calling this function!
 */
int ami_action_response_result(struct ami_session *ami, struct ami_response *resp);

/*!
 * \brief Get a variable
 * \param ami
 * \param variable Name of variable
 * \param channel Channel name, or NULL to get a global variable
 * \retval Variable value if it exists, NULL otherwise
 * \note Caller is responsible for freeing returned string using free() if it is non-NULL
 */
char *ami_action_getvar(struct ami_session *ami, const char *variable, const char *channel);

/*!
 * \brief Get a variable
 * \param ami
 * \param variable Name of variable
 * \param channel Channel name, or NULL to get a global variable
 * \param buf Buffer
 * \param len Buffer size
 * \retval 0 on success, -1 on failure
 */
int ami_action_getvar_buf(struct ami_session *ami, const char *variable, const char *channel, char *buf, size_t len);

/*!
 * \brief Set a variable
 * \param ami
 * \param variable Name of variable
 * \param value Value of variable
 * \param channel Channel name, or NULL to set a global variable
 * \retval 0 on success, -1 on failure
 */
int ami_action_setvar(struct ami_session *ami, const char *variable, const char *value, const char *channel);

/*!
 * \brief Originate a call to an extension
 * \param ami
 * \param dest Channel destination
 * \param context
 * \param exten
 * \param priority
 * \param callerid Caller ID if desired, or NULL for none
 * \retval 0 on success, -1 on failure
 */
int ami_action_originate_exten(struct ami_session *ami, const char *dest, const char *context, const char *exten, const char *priority, const char *callerid);

/*!
 * \brief Redirect a channel
 * \param ami
 * \param channel Channel name
 * \param context
 * \param exten
 * \param priority
 * \retval 0 on success, -1 on failure
 */
int ami_action_redirect(struct ami_session *ami, const char *channel, const char *context, const char *exten, const char *priority);

/*!
 * \brief Reload a module
 * \param ami
 * \param module Full name of module to reload
 * \retval 0 on success, -1 on failure
 */
int ami_action_reload(struct ami_session *ami, const char *module);
