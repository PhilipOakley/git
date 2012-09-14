/*
 * Copyright (C) 2012 Philipp A. Hartmann <pah@qo.cx>
 *
 * This file is licensed under the GPL v2, or a later version
 * at the discretion of Linus.
 *
 * This credential struct and API is simplified from git's
 * credential.{h,c} to be used within credential helper
 * implementations.
 */
#ifndef CREDENTIAL_HELPER_H_INCLUDED_
#define CREDENTIAL_HELPER_H_INCLUDED_

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>

struct credential
{
	char          *protocol;
	char          *host;
	unsigned short port;
	char          *path;
	char          *username;
	char          *password;
	char          *url;     /* <protocol>://[username@][host[:port]][/path] */
};

#define CREDENTIAL_INIT \
  { NULL,NULL,0,NULL,NULL,NULL,NULL }

void credential_init(struct credential *c);
void credential_clear(struct credential *c);
int  credential_read(struct credential *c);
void credential_write(const struct credential *c);

typedef int (*credential_op_cb)(struct credential*);

struct credential_operation
{
	char             *name;
	credential_op_cb op;
};

#define CREDENTIAL_OP_END \
  { NULL,NULL }

/*
 * Table with operation callbacks is defined in concrete
 * credential helper implementation and contains entries
 * like { "get", function_to_get_credential } terminated
 * by CREDENTIAL_OP_END.
 */
extern struct credential_operation const credential_helper_ops[];

/* ---------------- common helper functions ---------------- */

static inline void free_password(char *password)
{
	char *c = password;
	if (!password)
		return;

	while (*c) *c++ = '\0';
	free(password);
}

static inline void warning(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "warning: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n" );
	va_end(ap);
}

static inline void error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "error: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n" );
	va_end(ap);
}

static inline void die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap,fmt);
	error(fmt, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

static inline void die_errno(int err)
{
	error("%s", strerror(err));
	exit(EXIT_FAILURE);
}

static inline void *xmalloc(size_t size)
{
  void *ret = malloc(size);
	if (!ret && !size)
		ret = malloc(1);
	if (!ret)
		 die_errno(errno);

	return ret;
}

static inline char *xstrdup(const char *str)
{
	char *ret = strdup(str);
	if (!ret)
		die_errno(errno);

	return ret;
}

#ifndef NO_STRNDUP
static inline char *xstrndup(const char *str, size_t len)
{
	char *ret = strndup(str,len);
	if (!ret)
		die_errno(errno);

	return ret;
}
#endif

#endif /* CREDENTIAL_HELPER_H_INCLUDED_ */
