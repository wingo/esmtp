/**
 * \file smtp.h
 */

#ifndef _SMTP_H
#define _SMTP_H


#include <libesmtp.h>

#include "list.h"
#include "message.h"


/** 
 * \name Identity management
 */
/*@{*/

/**
 * Identity.
 */
typedef struct {
	struct list_head list;
	char *address;
	char *host;
	char *user;
	char *pass;
	enum starttls_option starttls;	/**< it should default to Starttls_DISABLED */
	char *certificate_passphrase;
} identity_t;

/** 
 * Default identity.
 */
extern identity_t *default_identity;

/** Create a new identity */
identity_t *identity_new(void);

/** Add a new identity */
void identity_add(identity_t *identity);

/** Lookup a identity */
identity_t *identity_lookup(const char *address);

/** Initialize the identities resources */
int identities_init(void);

/** Cleanup the resources associated with the identities */
void identities_cleanup(void);

/*@}*/


/** Send a message via a SMTP server */
int smtp_send(message_t *msg);

#endif
