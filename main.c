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
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include <auth-client.h>
#include <libesmtp.h>

char *from = NULL;
char *host = NULL;
char *user = NULL;
char *pass = NULL;
enum starttls_option starttls = Starttls_DISABLED;
char *certificate_passphrase = NULL;

extern void parse_rcfile(void);

/* Callback function to read the message from a file.  Since libESMTP
   does not provide callbacks which translate line endings, one must
   be provided by the application.

   The message is read a line at a time and the newlines converted
   to \r\n.  Unfortunately, RFC 822 states that bare \n and \r are
   acceptable in messages and that individually they do not constitute a
   line termination.  This requirement cannot be reconciled with storing
   messages with Unix line terminations.  RFC 2822 rescues this situation
   slightly by prohibiting lone \r and \n in messages.

   The following code cannot therefore work correctly in all situations.
   Furthermore it is very inefficient since it must search for the \n.
 */
#define BUFLEN	8192

const char *
readlinefp_cb (void **buf, int *len, void *arg)
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

void
monitor_cb (const char *buf, int buflen, int writing, void *arg)
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

void
usage (void)
{
  fputs ("usage: esmtp [options] mailbox [mailbox ...]\n",
         stderr);
}

void
version (void)
{
  char buf[32];

  smtp_version (buf, sizeof buf, 0);
  printf ("libESMTP version %s\n", buf);
}

/* Callback to request user/password info.  Not thread safe. */
int
authinteract (auth_client_request_t request, char **result, int fields,
              void *arg)
{
  int i;

  for (i = 0; i < fields; i++)
    {
      if (request[i].flags & AUTH_USER && user)
	result[i] = user;
      else if (request[i].flags & AUTH_PASS && pass)
	result[i] = pass;
      else
	return 0;
    }
  return 1;
}

int
tlsinteract (char *buf, int buflen, int rwflag, void *arg)
{
  char *pw;
  int len;

  if (certificate_passphrase)
    {
      pw = certificate_passphrase;
      len = strlen (pw);
      if (len + 1 > buflen)
	return 0;
      strcpy (buf, pw);
      return len;
    }
  else
    return 0;
}

/* Callback to print the recipient status */
void
print_recipient_status (smtp_recipient_t recipient,
			const char *mailbox, void *arg)
{
  const smtp_status_t *status;

  status = smtp_recipient_status (recipient);
  fprintf (stderr, "%s: %d %s\n", mailbox, status->code, status->text);
}

int
main (int argc, char **argv)
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

  /* Parse the rc file. */
  parse_rcfile();

  /* This program sends only one message at a time.  Create an SMTP
     session and add a message to it. */
  auth_client_init ();
  session = smtp_create_session ();
  message = smtp_add_message (session);

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
        break;

      case 'V':
	/* Set original envelope id */
        break;

      case 'b':
	/* Operations mode */
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
	      /* Forground queue run */
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
	exit (2);
        break;

      case 'v':
	/* Verbose */
        smtp_set_monitorcb (session, monitor_cb, stdout, 1);
	break;
	
      default:
        usage ();
        exit (2);
      }

  /* At least one more argument is needed.
   */
  if (optind > argc - 1)
    {
      usage ();
      exit (2);
    }

  /* NB.  libESMTP sets timeouts as it progresses through the protocol.
     In addition the remote server might close its socket on a timeout.
     Consequently libESMTP may sometimes try to write to a socket with
     no reader.  Ignore SIGPIPE, then the program doesn't get killed
     if/when this happens. */
  sa.sa_handler = SIG_IGN;
  sigemptyset (&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction (SIGPIPE, &sa, NULL); 

  /* Set the SMTP Starttls extension. */
  smtp_starttls_enable (session, starttls);

  /* Set the host running the SMTP server.  LibESMTP has a default port
     number of 587, however this is not widely deployed so the port
     is specified as 25 along with the default MTA host. */
  smtp_set_server (session, host ? host : "localhost:25");

  /* Do what's needed at application level to use authentication.
   */
  authctx = auth_create_context ();
  auth_set_mechanism_flags (authctx, AUTH_PLUGIN_PLAIN, 0);
  auth_set_interact_cb (authctx, authinteract, NULL);

  /* Use our callback for X.509 certificate passwords.  If STARTTLS is
     not in use or disabled in configure, the following is harmless. */
  smtp_starttls_set_password_cb (tlsinteract, NULL);

  /* Now tell libESMTP it can use the SMTP AUTH extension.
   */
  smtp_auth_set_context (session, authctx);

  /* Set the reverse path for the mail envelope.  (NULL is ok)
   */
  smtp_set_reverse_path (message, from);

  /* Open the message file and set the callback to read it.
   */
#if 0
  smtp_set_message_fp (message, stdin);
#else
  smtp_set_messagecb (message, readlinefp_cb, stdin);
#endif

  /* At present it can't handle one recipient only out of many
     failing.  Make libESMTP require all specified recipients to
     succeed before transferring a message.  */
  smtp_option_require_all_recipients (session, 1);
    
  /* Add remaining program arguments as message recipients.
   */
  while (optind < argc)
    {
      recipient = smtp_add_recipient (message, argv[optind++]);

      /* Recipient options set here */
      if (notify != Notify_NOTSET)
        smtp_dsn_set_notify (recipient, notify);
    }

  /* Initiate a connection to the SMTP server and transfer the
     message. */
  if (!smtp_start_session (session))
    {
      char buf[128];

      fprintf (stderr, "SMTP server problem %s\n",
	       smtp_strerror (smtp_errno (), buf, sizeof buf));
      
      ret = 1;
    }
  else
    {
      /* Report on the success or otherwise of the mail transfer.
       */
      status = smtp_message_transfer_status (message);
      if (status->code / 100 == 2)
        {
	  ret = 0;
      	}
      else
        {
	  /* Report on the failure of the mail transfer.
	   */
	  status = smtp_message_transfer_status (message);
	  fprintf (stderr, "%d %s\n", status->code, status->text);
	  smtp_enumerate_recipients (message, print_recipient_status, NULL);
	  
	  ret = 1;
	}
    }
  
  /* Free resources consumed by the program.
   */
  smtp_destroy_session (session);
  auth_destroy_context (authctx);
  auth_client_exit ();
  exit (ret);
}

