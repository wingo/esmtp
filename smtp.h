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
	
	char *address;	/**< reverse path address */

	char *host;	/**< hostname and service (port) */

	/** \name Auth Extension */
	/*@{*/
	char *user;
	char *pass;
	/*@}*/

	/** \name StartTLS Extension */
	/*@{*/
	enum starttls_option starttls;
	char *certificate_passphrase;
	/*@}*/

	/** \name Pre-connect Command */
	/*@{*/
	char *preconnect;
	/*@}*/
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
void identities_init(void);

/** Cleanup the resources associated with the identities */
void identities_cleanup(void);

/*@}*/


/** Send a message via a SMTP server */
void smtp_send(message_t *msg);

#endif
