/*
 *  A libESMTP Example Application.
 *  Copyright (C) 2001,2002  Brian Stafford <brian@stafford.uklinux.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published
 *  by the Free Software Foundation; either version 2 of the License,
 *  or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* This program attemps to mimic the sendmail program behavior using libESMTP.
 *
 * Adapted from the libESMTP's mail-file example by José Fonseca.
 */
#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include <auth-client.h>
#include <libesmtp.h>

#include "esmtp.h"

/* Identity management */
identity_t default_identity = {
	NULL,
	NULL,
	NULL,
	NULL,
	Starttls_DISABLED,
	NULL
};

identity_list_t *identities_head = NULL, **identities_tail = &identities_head;

/* Callback function to read the message from a file.  Since libESMTP does not
 * provide callbacks which translate line endings, one must be provided by the
 * application.
 *
 * The message is read a line at a time and the newlines converted to \r\n.
 * Unfortunately, RFC 822 states that bare \n and \r are acceptable in messages
 * and that individually they do not constitute a line termination.  This
 * requirement cannot be reconciled with storing messages with Unix line
 * terminations.  RFC 2822 rescues this situation slightly by prohibiting lone
 * \r and \n in messages.
 *
 * The following code cannot therefore work correctly in all situations.
 * Furthermore it is very inefficient since it must search for the \n.
 */
#define BUFLEN	8192

const char * readlinefp_cb (void **buf, int *len, void *arg)
{
    int octets;

    if (*buf == NULL)
	*buf = malloc (BUFLEN);

    if (len == NULL)
    {
	rewind ((FILE *) arg);
	return NULL;
    }

    if (fgets (*buf, BUFLEN - 2, (FILE *) arg) == NULL)
	octets = 0;
    else
    {
	char *p = strchr (*buf, '\0');

	if (p[-1] == '\n' && p[-2] != '\r')
	{
	    strcpy (p - 1, "\r\n");
	    p++;
	}
	octets = p - (char *) *buf;
    }
    *len = octets;
    return *buf;
}

#define SIZETICKER 1024	/* print 1 dot per this many bytes */

void event_cb (smtp_session_t session, int event_no, void *arg, ...)
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
	    fputs("Disconnected\n", fp);
	    break;
	
	default:
	    break;
    }
    va_end (ap);
}

void monitor_cb (const char *buf, int buflen, int writing, void *arg)
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

void usage (void)
{
    fputs ("usage: esmtp [options] mailbox [mailbox ...]\n",
	   stderr);
}

void version (void)
{
    char buf[32];

    smtp_version (buf, sizeof buf, 0);
    printf ("libESMTP version %s\n", buf);
}

/* Callback to request user/password info.  Not thread safe. */
int authinteract (auth_client_request_t request, char **result, int fields,
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

int tlsinteract (char *buf, int buflen, int rwflag, void *arg)
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

/* Callback to print the recipient status. */
void print_recipient_status (smtp_recipient_t recipient, const char *mailbox, 
			     void *arg)
{
    const smtp_status_t *status;

    status = smtp_recipient_status (recipient);
    fprintf (stderr, "%s: %d %s\n", mailbox, status->code, status->text);
}

int main (int argc, char **argv)
{
    smtp_session_t session;
    smtp_message_t message;
    smtp_recipient_t recipient;
    auth_context_t authctx;
    const smtp_status_t *status;
    struct sigaction sa;
    int c;
    int ret;
    enum notify_flags notify = Notify_NOTSET;
    FILE *fp = NULL;
    identity_t *identity = &default_identity;
    char *from = NULL;
    identity_list_t *p;

    /* Parse the rc file. */
    parse_rcfile();

    /* This program sends only one message at a time.  Create an SMTP session.
     */
    auth_client_init ();
    session = smtp_create_session ();

    while ((c = getopt (argc, argv,
			"A:B:b:C:cd:e:F:f:Gh:IiL:M:mN:nO:o:p:q:R:r:sTtV:vX:")) != EOF)
	switch (c)
	{
	    case 'A':
		/* Use alternate sendmail/submit.cf */
		break;

	    case 'B':
		/* Body type */
		break;

	    case 'C':
		/* Select configuration file */
		rcfile = optarg;
		break;

	    case 'F':
		/* Set full name */
		break;

	    case 'G':
		/* Relay (gateway) submission */
		break;

	    case 'I':
		/* Initialize alias database */
		break;

	    case 'L':
		/* Program label */
		break;

	    case 'M':
		/* Define macro */
		break;

	    case 'N':
		/* Delivery status notifications */
		if (strcmp (optarg, "never") == 0)
		    notify = Notify_NEVER;
		else
		{
		    if (strstr (optarg, "failure"))
			notify |= Notify_FAILURE;
		    if (strstr (optarg, "delay"))
			notify |= Notify_DELAY;
		    if (strstr (optarg, "success"))
			notify |= Notify_SUCCESS;
		}
		break;

	    case 'R':
		/* What to return */
		break;

	    case 'T':
		/* Set timeout interval */
		break;

	    case 'X':
		/* Traffic log file */
		if (fp)
		    fclose(fp);
		if ((fp = fopen(optarg, "a")))
		    /* Add a protocol monitor. */
		    smtp_set_monitorcb (session, monitor_cb, fp, 1);
		break;

	    case 'V':
		/* Set original envelope id */
		break;

	    case 'b':
		/* Operations mode */
		c = (optarg == NULL) ? ' ' : *optarg;
		switch (c)
		{
		    case 'm':
			/* Deliver mail in the usual way */
			break;

		    case 'i':
			/* Initialize the alias database */
			break;

		    case 'a':
			/* Go into ARPANET mode */
		    case 'd':
			/* Run as a daemon */
		    case 'D':
			/* Run as a daemon in foreground */
		    case 'h':
			/* Print the persistent host status database */
		    case 'H':
			/* Purge expired entries from the persistent host
			 * status database */
		    case 'p':
			/* Print a listing of the queue(s) */
		    case 'P':
			/* Print number of entries in the queue(s) */
		    case 's':
			/* Use the SMTP protocol as described in RFC821
			 * on standard input and output */
		    case 't':
			/* Run in address test mode */
		    case 'v':
			/* Verify names only */
			fprintf (stderr, "Unsupported operation mode %c\n", c);
			exit (64);
			break;

		    default:
			fprintf (stderr, "Invalid operation mode %c\n", c);
			exit (64);
			break;
		 }
		 break;

	    case 'c':
		/* Connect to non-local mailers */
		break;

	    case 'd':
		/* Debugging */
		break;

	    case 'e':
		/* Error message disposition */
		break;

	    case 'f':
		/* From address */
	    case 'r':
		/* Obsolete -f flag */
		from = optarg;
		p = identities_head;
		while(p)
		{
		    if(!strcmp(p->identity.identity, from))
		    {
			identity = &p->identity;
			break;
		    }
		    p = p->next;
		}
		break;

	    case 'h':
		/* Hop count */
		break;

	    case 'i':
		/* Don't let dot stop me */
		break;

	    case 'm':
		/* Send to me too */
		break;

	    case 'n':
		/* don't alias */
		break;

	    case 'o':
		/* Set option */
		break;

	    case 'p':
		/* Set protocol */
		break;

	    case 'q':
		/* Run queue files at intervals */
		if (optarg[0] == '!')
		{
		    /* Negate the meaning of pattern match */
		    optarg++;
		}

		switch (optarg[0])
		{
		    case 'G':
			/* Limit by queue group name */
			break;

		    case 'I':
			/* Limit by ID */
			break;

		    case 'R':
			/* Limit by recipient */
			break;

		    case 'S':
			/* Limit by sender */
			break;

		    case 'f':
			/* Foreground queue run */
			break;

		    case 'p':
			/* Persistent queue */
			if (optarg[1] == '\0')
			    break;
			++optarg;

		    default:
			break;
		}
		break;

	    case 's':
		/* Save From lines in headers */
		break;

	    case 't':
		/* Read recipients from message */
		fprintf (stderr, "Unsupported option 't'\n");
		exit (64);
		break;

	    case 'v':
		/* Verbose */
		/* Set the event callback. */
		smtp_set_eventcb (session, event_cb, stdout);
		break;

	    default:
		usage ();
		exit (64);
	    }

    /* At least one more argument is needed. */
    if (optind > argc - 1)
    {
	usage ();
	exit (64);
    }

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
    smtp_set_reverse_path (message, from);

    /* Open the message file and set the callback to read it. */
#if 0
    smtp_set_message_fp (message, stdin);
#else
    smtp_set_messagecb (message, readlinefp_cb, stdin);
#endif

    /* Add remaining program arguments as message recipients. */
    while (optind < argc)
    {
	recipient = smtp_add_recipient (message, argv[optind++]);

	/* Recipient options set here */
	if (notify != Notify_NOTSET)
	    smtp_dsn_set_notify (recipient, notify);
    }

    /* Initiate a connection to the SMTP server and transfer the message. */
    if (!smtp_start_session (session))
    {
	char buf[128];

	fprintf (stderr, "SMTP server problem %s\n",
		 smtp_strerror (smtp_errno (), buf, sizeof buf));

	ret = 69;
    }
    else
    {
	/* Report on the success or otherwise of the mail transfer. */
	status = smtp_message_transfer_status (message);
	if (status->code / 100 == 2)
	{
	    ret = 0;
      	}
	else
	{
	    /* Report on the failure of the mail transfer. */
	    status = smtp_message_transfer_status (message);
	    fprintf (stderr, "%d %s\n", status->code, status->text);
	    smtp_enumerate_recipients (message, print_recipient_status, NULL);

	    ret = 70;
	}
    }

    /* Free resources consumed by the program. */
    if (fp)
    {
	fputc('\n', fp);
	fclose(fp);
    }

    smtp_destroy_session (session);
    auth_destroy_context (authctx);
    auth_client_exit ();

    exit (ret);
}
