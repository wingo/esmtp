/**
 * \file local.c
 * Local delivery of mail via a MDA.
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <pwd.h>

#include "local.h"
#include "main.h"


#if 0
char *mda = NULL;
#else
char *mda = "/usr/bin/procmail -d %T";
#endif

FILE *mda_fp = NULL;


int local_address(const char *address)
{
	return !strchr(address, '@');
}

/** replace ' by _ */
static void sanitize(char *s)
{
	char *cp;

	for (cp = s; (cp = strchr (cp, '\'')); cp++)
			*cp = '_';
}

#define xmalloc malloc
#define xstrdup strdup

#define HAVE_SETEUID

/**
 * Pipe the message to the MDA for local delivery.
 *
 * Based on fetchmail's open_mda_sink().
 */
int local_init(message_t *message)
{
#ifdef HAVE_SETEUID
	uid_t orig_uid, uid;
	struct passwd *pw;
#endif /* HAVE_SETEUID */
	struct		idlist *idp;
	int		length = 0, fromlen = 0, nameslen = 0;
	char		*names = NULL, *before, *after, *from = NULL;
	char *user = NULL;

	if (!mda)
			return 1;

	length = strlen(mda);
	before = xstrdup(mda);

	/* get user addresses for %T */
	if (strstr(before, "%T"))
	{
		struct list_head *ptr;

		/*
		 * We go through this in order to be able to handle very
		 * long lists of users and (re)implement %s.
		 */
		nameslen = 0;
		list_for_each(ptr, &message->local_recipients)
		{
			recipient_t *recipient = list_entry(ptr, recipient_t, list);
			
			if(recipient->address)
				nameslen += (strlen(recipient->address) + 1);	/* string + ' ' */
		}

		names = (char *)xmalloc(nameslen + 1);		/* account for '\0' */
		names[0] = '\0';
		list_for_each(ptr, &message->local_recipients)
		{
			recipient_t *recipient = list_entry(ptr, recipient_t, list);
			
			if(recipient->address)
			{
				if(!user)
					user = recipient->address;
				strcat(names, recipient->address);
				strcat(names, " ");
			}
		}
		names[--nameslen] = '\0';		/* chop trailing space */

		sanitize(names);
	}

	/* get From address for %F */
	if (strstr(before, "%F"))
	{
		from = xstrdup(message->reverse_path);

		sanitize(from);

		fromlen = strlen(from);
	}

	/* do we have to build an mda string? */
	if (names || from) 
	{				
		char		*sp, *dp;

		/* find length of resulting mda string */
		sp = before;
		while ((sp = strstr(sp, "%s"))) {
			length += nameslen;		/* subtract %s and add '' */
			sp += 2;
		}
		sp = before;
		while ((sp = strstr(sp, "%T"))) {
			length += nameslen;		/* subtract %T and add '' */
			sp += 2;
		}
		sp = before;
		while ((sp = strstr(sp, "%F"))) {
			length += fromlen;		/* subtract %F and add '' */
			sp += 2;
		}

		after = xmalloc(length + 1);

		/* copy mda source string to after, while expanding %[sTF] */
		for (dp = after, sp = before; (*dp = *sp); dp++, sp++) {
			if (sp[0] != '%')		continue;

			/* need to expand? BTW, no here overflow, because in
			** the worst case (end of string) sp[1] == '\0' */
			if (sp[1] == 'T') {
				*dp++ = '\'';
				strcpy(dp, names);
				dp += nameslen;
				*dp++ = '\'';
				sp++;		/* position sp over [sT] */
				dp--;		/* adjust dp */
			} else if (sp[1] == 'F') {
				*dp++ = '\'';
				strcpy(dp, from);
				dp += fromlen;
				*dp++ = '\'';
				sp++;		/* position sp over F */
				dp--;		/* adjust dp */
			}
		}

		if (names) {
			free(names);
			names = NULL;
		}
		if (from) {
			free(from);
			from = NULL;
		}

		free(before);

		before = after;
	}

#ifdef HAVE_SETEUID
	/*
	 * Arrange to run with user's permissions if we're root.
	 * This will initialize the ownership of any files the
	 * MDA creates properly.  (The seteuid call is available
	 * under all BSDs and Linux)
	 */
	orig_uid = getuid();
	/* if `user' doesn't name a real local user, try to run as root */
	if ((pw = getpwnam(user)) == (struct passwd *)NULL)
		uid = 0;
	else
		uid = pw->pw_uid;	/* for local delivery via MDA */
	seteuid(uid);
#endif /* HAVE_SETEUID */

	mda_fp = popen(before, "w");

#ifdef HAVE_SETEUID
	/* this will fail quietly if we didn't start as root */
	seteuid(orig_uid);
#endif /* HAVE_SETEUID */

	if (!mda_fp)
	{
	free(before);
	before = NULL;
		fprintf(stderr, "MDA open failed\n");
		return 0;
	}

	if(verbose)
		fprintf(stdout, "Connected to MDA: %s\n", before);

	free(before);
	before = NULL;
	return 1;
}

int local_flush(message_t *message)
{
	char buffer[BUFSIZ];
	size_t n;

	do {
		n = message_read(message, buffer, BUFSIZ);
		if(fwrite(buffer, 1, n, mda_fp) != n)
			return 0;
	} while(n == BUFSIZ);
	
	return 1;
}

void local_cleanup(void)
{
	if(mda_fp)
	{
		fclose(mda_fp);
		mda_fp = NULL;

		if(verbose)
			fprintf(stdout, "Disconnected to MDA\n");
	}
}
