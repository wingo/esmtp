/**
 * \file message.h
 * Simple message handling.
 * 
 * \author José Fonseca
 */

#ifndef _MESSAGE_H
#define _MESSAGE_H


#include <libesmtp.h>

#include "list.h"


/**
 * Item of the recipient list.
 */
typedef struct {
	struct list_head list;
	char *address;
} recipient_t;

/**
 * A message.
 */
typedef struct {
	char *reverse_path;	/**< reverse path for the mail envelope */
	struct list_head remote_recipients;	/**< remote recipients */
	struct list_head local_recipients;	/**< local recipients */

	enum notify_flags notify;	/**< libESMTP notificiation flags */
    
	/** \name buffering */
	/*@{*/
	char *buffer;
	size_t buffer_size;
	size_t buffer_start, buffer_stop;
	int buffer_r;		/**< whether the last character was a '\r' */
	/*@}*/
	
	FILE *fp;		/**< message file pointer */
} message_t;

/** Create a new message. */
message_t *message_new(void);

/** Free the resources associated with a message. */
void message_free(message_t *m);

int message_set_reverse_path(message_t *message, const char *address);

int message_add_recipient(message_t *message, const char *address);

int message_parse_headers(message_t *message);

size_t message_read(message_t *message, char *ptr, size_t size);

void message_rewind(message_t *message);

int message_eof(message_t *message);

#endif
