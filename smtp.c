/**
 * \file smtp.c
 * Send a message via libESMTP.
 *
 * \author Adapted from the libESMTP's mail-file example by Jos� Fonseca.
 */


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include <auth-client.h>
#include <libesmtp.h>

#include "smtp.h"
#include "main.h"
#include "xmalloc.h"


/**
 * \name Identity management
 */
/*@{*/

identity_t *default_identity = NULL;

static LIST_HEAD(identities);

identity_t *identity_new(void)
{
	identity_t *identity;

	identity = (identity_t *)xmalloc(sizeof(identity_t));

	memset(identity, 0, sizeof(identity_t));

	identity->starttls = Starttls_DISABLED;
		
	return identity;
}

void identity_free(identity_t *identity)
{
	if(identity->address)
		free(identity->address);

	if(identity->host)
		free(identity->host);

	if(identity->user)
		free(identity->user);

	if(identity->pass)
		free(identity->pass);

	if(identity->certificate_passphrase)
		free(identity->certificate_passphrase);

	free(identity);
}

void identity_add(identity_t *identity)
{
	list_add(&identity->list, &identities);
}

identity_t *identity_lookup(const char *address)
{
	if(address)
	{
		struct list_head *ptr;

		list_for_each(ptr, &identities)
		{
			identity_t *identity;
			
			identity = list_entry(ptr, identity_t, list);
			if(!strcmp(identity->address, address))
				return identity;
		}
	}

	return default_identity;
}
	
void identities_init(void)
{
	default_identity = identity_new();
}

void identities_cleanup(void)
{
	if(default_identity)
		identity_free(default_identity);

	if(!list_empty(&identities))
	{
		struct list_head *ptr, *tmp;
	
		list_for_each_safe(ptr, tmp, &identities)
		{
			identity_t *identity;
			
			identity = list_entry(ptr, identity_t, list);
			list_del(ptr);
			
			identity_free(identity);
		}

	}
}
	
/*@}*/


/**
 * \name libESMTP interface
 */
/*@{*/

/**
 * Callback function to read the message from a file.  
 *
 * Since libESMTP does not provide callbacks which translate line endings, one
 * must be provided by the application.
 */
static const char * message_cb (void **buf, int *len, void *arg)
{
	message_t *message = (message_t *)arg;
	int octets;

	if (len == NULL)
		assert(*buf == NULL);

	if (*buf == NULL)
		*buf = malloc (BUFSIZ);

	if (len == NULL)
	{
		message_rewind(message);
		return NULL;
	}

	*len = message_read(message, *buf, BUFSIZ);
	
	return *buf;
}

#define SIZETICKER 1024		/**< print 1 dot per this many bytes */

static void event_cb (smtp_session_t session, int event_no, void *arg, ...)
{
	FILE *fp = arg;
	va_list ap;
	const char *mailbox;
	smtp_message_t message;
	smtp_recipient_t recipient;
	const smtp_status_t *status;
	static int sizeticking = 0, sizeticker;

	if (event_no != SMTP_EV_MESSAGEDATA && sizeticking)
	{
		fputs("\n", fp);
		sizeticking = 0;
	}

	va_start (ap, arg);
	switch (event_no) {
		case SMTP_EV_CONNECT:
			fputs("Connected to MTA\n", fp);
			break;
			
		case SMTP_EV_MAILSTATUS:
			mailbox = va_arg (ap, const char *);
			message = va_arg (ap, smtp_message_t);
			status = smtp_reverse_path_status (message);
			fprintf (fp, "From %s: %d %s", mailbox, status->code, status->text);
			break;

		case SMTP_EV_RCPTSTATUS:
			mailbox = va_arg (ap, const char *);
			recipient = va_arg (ap, smtp_recipient_t);
			status = smtp_recipient_status (recipient);
			fprintf (fp, "To %s: %d %s", mailbox, status->code, status->text);
			break;
			
		case SMTP_EV_MESSAGEDATA:
			message = va_arg (ap, smtp_message_t);
			if (!sizeticking)
			{
				fputs("Message data: ", fp);
				sizeticking = 1;
				sizeticker = SIZETICKER - 1;
			}
			sizeticker += va_arg (ap, int);
			while (sizeticker >= SIZETICKER)
			{
				fputc('.', fp);
				sizeticker -= SIZETICKER;
			}
			break;

		case SMTP_EV_MESSAGESENT:
			message = va_arg (ap, smtp_message_t);
			status = smtp_message_transfer_status (message);
			fprintf (fp, "Message sent: %d %s", status->code, status->text);
			break;
			
		case SMTP_EV_DISCONNECT:
			fputs("Disconnected to MTA\n", fp);
			break;
		
		default:
			break;
	}
	va_end (ap);
}

static void monitor_cb (const char *buf, int buflen, int writing, void *arg)
{
	FILE *fp = arg;

	if (writing == SMTP_CB_HEADERS)
	{
		fputs ("H: ", fp);
		fwrite (buf, 1, buflen, fp);
		return;
	}
	
	fputs (writing ? "C: " : "S: ", fp);
	fwrite (buf, 1, buflen, fp);
	if (buf[buflen - 1] != '\n')
		putc ('\n', fp);
}

/**
 * Callback to request user/password info.  
 *
 * Not thread safe. 
 */
static int authinteract (auth_client_request_t request, char **result, int fields,
				  void *arg)
{
	identity_t *identity = (identity_t *)arg;
	int i;

	if(!identity)
		return 0;
	
	for (i = 0; i < fields; i++)
	{
		if (request[i].flags & AUTH_USER && identity->user)
			result[i] = identity->user;
		else if (request[i].flags & AUTH_PASS && identity->pass)
			result[i] = identity->pass;
		else
			return 0;
	}
	return 1;
}

static int tlsinteract (char *buf, int buflen, int rwflag, void *arg)
{
	identity_t *identity = (identity_t *)arg;
	char *pw;
	int len;

	if(!identity)
		return 0;
	
	if (identity->certificate_passphrase)
	{
		pw = identity->certificate_passphrase;
		len = strlen (pw);
		if (len + 1 > buflen)
			return 0;
		strcpy (buf, pw);
		return len;
	}
	else
		return 0;
}

/**
 * Callback to print the recipient status.
 */
void print_recipient_status (smtp_recipient_t recipient, const char *mailbox, 
							 void *arg)
{
	const smtp_status_t *status;

	status = smtp_recipient_status (recipient);
	fprintf (stderr, "%s: %d %s\n", mailbox, status->code, status->text);
}

void smtp_send(message_t *msg)
{
	smtp_session_t session;
	smtp_message_t message;
	smtp_recipient_t recipient;
	auth_context_t authctx;
	const smtp_status_t *status;
	struct sigaction sa;
	int ret;
	identity_t *identity;
	struct list_head *ptr;
	
	/* This program sends only one message at a time.  Create an SMTP
	 * session.
	 */
	auth_client_init ();
	session = smtp_create_session ();

	/* Add a protocol monitor. */
	if(log_fp)
		smtp_set_monitorcb (session, monitor_cb, log_fp, 1);

	/* Lookup the identity */
	identity = identity_lookup(msg->reverse_path); 
	assert(identity);

	/* Set the event callback. */
	if(verbose)
		smtp_set_eventcb (session, event_cb, stdout);

	/* NB.  libESMTP sets timeouts as it progresses through the protocol.  In
	 * addition the remote server might close its socket on a timeout.
	 * Consequently libESMTP may sometimes try to write to a socket with no
	 * reader.  Ignore SIGPIPE, then the program doesn't get killed if/when
	 * this happens.
	 */
	sa.sa_handler = SIG_IGN; sigemptyset (&sa.sa_mask); sa.sa_flags = 0;
	sigaction (SIGPIPE, &sa, NULL);

	/* Set the host running the SMTP server.  LibESMTP has a default port
	 * number of 587, however this is not widely deployed so the port is
	 * specified as 25 along with the default MTA host.
	 */
	smtp_set_server (session, identity->host ? identity->host : "localhost:25");

	/* Set the SMTP Starttls extension. */
	smtp_starttls_enable (session, identity->starttls);

	/* Do what's needed at application level to use authentication. */
	authctx = auth_create_context ();
	auth_set_mechanism_flags (authctx, AUTH_PLUGIN_PLAIN, 0);
	auth_set_interact_cb (authctx, authinteract, identity);

	/* Use our callback for X.509 certificate passwords.  If STARTTLS is not in
	 * use or disabled in configure, the following is harmless.
	 */
	smtp_starttls_set_password_cb (tlsinteract, identity);

	/* Now tell libESMTP it can use the SMTP AUTH extension. */
	smtp_auth_set_context (session, authctx);

	/* At present it can't handle one recipient only out of many failing.  Make
	 * libESMTP require all specified recipients to succeed before transferring
	 * a message.
	 */
	smtp_option_require_all_recipients (session, 1);

	/* Add a message to the SMTP session. */
	message = smtp_add_message (session);

	/* Set the reverse path for the mail envelope.  (NULL is ok) */
	smtp_set_reverse_path (message, msg->reverse_path);

	/* Open the message file and set the callback to read it. */
	smtp_set_messagecb (message, message_cb, msg);

	/* Add remaining program arguments as message recipients. */
	list_for_each(ptr, &msg->remote_recipients)
	{
		recipient_t *entry = list_entry(ptr, recipient_t, list);
		
		if(entry->address)
		{
			recipient = smtp_add_recipient (message, entry->address);
			
			/* Recipient options set here */
			if (msg->notify != Notify_NOTSET)
				smtp_dsn_set_notify (recipient, msg->notify);
		}
	}

	/* Initiate a connection to the SMTP server and transfer the message. */
	if (!smtp_start_session (session))
	{
		char buf[128];

		fprintf (stderr, "SMTP server problem %s\n",
				 smtp_strerror (smtp_errno (), buf, sizeof(buf)));

		ret = EX_UNAVAILABLE;
	}
	else
	{
		/* Report on the success or otherwise of the mail transfer. */
		status = smtp_message_transfer_status (message);
		if (status->code / 100 == 2)
			ret = EX_OK;
		else
		{
			/* Report on the failure of the mail transfer. */
			status = smtp_message_transfer_status (message);
			fprintf (stderr, "%d %s\n", status->code, status->text);
			smtp_enumerate_recipients (message, print_recipient_status, NULL);

			ret = EX_SOFTWARE;
		}
	}

	if (log_fp)
		fputc('\n', log_fp);

	smtp_destroy_session (session);
	auth_destroy_context (authctx);
	auth_client_exit ();

	if (ret != EX_OK)
		exit(ret);
}

/*@}*/