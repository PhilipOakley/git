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

#include <credential_helper.h>

#ifdef WIN32
#include <fcntl.h>
#include <io.h>
#endif

void credential_init(struct credential *c)
{
	memset(c, 0, sizeof(*c));
}

void credential_clear(struct credential *c)
{
	free(c->protocol);
	free(c->host);
	free(c->path);
	free(c->username);
	free_password(c->password);
	free(c->url);

	credential_init(c);
}

int credential_read(struct credential *c)
{
	char    buf[1024];
	ssize_t line_len = 0;
	char   *key      = buf;
	char   *value;

	while (fgets(buf, sizeof(buf), stdin))
	{
		line_len = strlen(buf);

		if(buf[line_len-1]=='\n')
			buf[--line_len]='\0';

		if(!line_len)
			break;

		value = strchr(buf,'=');
		if(!value) {
			warning("invalid credential line: %s", key);
			return -1;
		}
		*value++ = '\0';

		if (!strcmp(key, "protocol")) {
			free(c->protocol);
			c->protocol = xstrdup(value);
		} else if (!strcmp(key, "host")) {
			free(c->host);
			c->host = xstrdup(value);
			value = strrchr(c->host,':');
			if (value) {
				*value++ = '\0';
				c->port = atoi(value);
			}
		} else if (!strcmp(key, "path")) {
			free(c->path);
			c->path = xstrdup(value);
		} else if (!strcmp(key, "username")) {
			free(c->username);
			c->username = xstrdup(value);
		} else if (!strcmp(key, "password")) {
			free_password(c->password);
			c->password = xstrdup(value);
			while (*value) *value++ = '\0';
		}
		/*
		 * Ignore other lines; we don't know what they mean, but
		 * this future-proofs us when later versions of git do
		 * learn new lines, and the helpers are updated to match.
		 */
	}

	/* Rebuild URI from parts */
	*buf = '\0';
	if (c->protocol) {
		strncat(buf, c->protocol, sizeof(buf));
		strncat(buf, "://", sizeof(buf));
	}
	if (c->username) {
		strncat(buf, c->username, sizeof(buf));
		strncat(buf, "@", sizeof(buf));
	}
	if (c->host)
		strncat(buf, c->host, sizeof(buf));
	if (c->port) {
		value = buf + strlen(buf);
		snprintf(value, sizeof(buf)-(value-buf), ":%hd", c->port);
	}
	if (c->path) {
		strncat(buf, "/", sizeof(buf));
		strncat(buf, c->path, sizeof(buf));
	}
	c->url = xstrdup(buf);

	return 0;
}

void credential_write_item(FILE *fp, const char *key, const char *value)
{
	if (!value)
		return;
	fprintf(fp, "%s=%s\n", key, value);
}

void credential_write(const struct credential *c)
{
	/* only write username/password, if set */
	credential_write_item(stdout, "username", c->username);
	credential_write_item(stdout, "password", c->password);
}

static void usage(const char *name)
{
	struct credential_operation const *try_op = credential_helper_ops;
	const char *basename = strrchr(name,'/');

	basename = (basename) ? basename + 1 : name;
	fprintf(stderr, "Usage: %s <", basename);
	while(try_op->name) {
		fprintf(stderr,"%s",(try_op++)->name);
		if(try_op->name)
			fprintf(stderr,"%s","|");
	}
	fprintf(stderr,"%s",">\n");
}

/*
 * generic main function for credential helpers
 */
int main(int argc, char *argv[])
{
	int ret = EXIT_SUCCESS;

	struct credential_operation const *try_op = credential_helper_ops;
	struct credential                  cred   = CREDENTIAL_INIT;

	if (!argv[1]) {
		usage(argv[0]);
		goto out;
	}

#ifdef WIN32
	/* git on Windows uses binary pipes to avoid CRLF-issues */
	_setmode(_fileno(stdin), _O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
#endif

	/* lookup operation callback */
	while(try_op->name && strcmp(argv[1], try_op->name))
		try_op++;

	/* unsupported operation given -- ignore silently */
	if(!try_op->name || !try_op->op)
		goto out;

	ret = credential_read(&cred);
	if(ret)
		goto out;

	if (!cred.protocol || !(cred.host || cred.path)) {
		ret = EXIT_FAILURE;
		goto out;
	}

	/* perform credential operation */
	ret = (*try_op->op)(&cred);

	credential_write(&cred);
out:
	credential_clear(&cred);
	return ret;
}
