/*
 * Copyright (C) 2011 John Szakmeister <john@szakmeister.net>
 *               2012 Philipp A. Hartmann <pah@qo.cx>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

/*
 * Credits:
 * - GNOME Keyring API handling originally written by John Szakmeister
 * - ported to credential helper API by Philipp A. Hartmann
 */

#include <credential_helper.h>
#include <gnome-keyring.h>

/* create a special keyring option string, if path is given */
static char* keyring_object(struct credential *c)
{
	char* object = NULL;

	if (!c->path)
		return object;

	object = (char*) malloc(strlen(c->host)+strlen(c->path)+8);
	if(!object)
		die_errno(errno);

	if(c->port)
		sprintf(object,"%s:%hd/%s",c->host,c->port,c->path);
	else
		sprintf(object,"%s/%s",c->host,c->path);

	return object;
}

int keyring_get(struct credential *c)
{
	char* object = NULL;
	GList *entries;
	GnomeKeyringNetworkPasswordData *password_data;
	GnomeKeyringResult result;

	if (!c->protocol || !(c->host || c->path))
		return EXIT_FAILURE;

	object = keyring_object(c);

	result = gnome_keyring_find_network_password_sync(
				c->username,
				NULL /* domain */,
				c->host,
				object,
				c->protocol,
				NULL /* authtype */,
				c->port,
				&entries);

	free(object);

	if (result == GNOME_KEYRING_RESULT_NO_MATCH)
		return EXIT_SUCCESS;

	if (result == GNOME_KEYRING_RESULT_CANCELLED)
		return EXIT_SUCCESS;

	if (result != GNOME_KEYRING_RESULT_OK) {
		error("%s",gnome_keyring_result_to_message(result));
		return EXIT_FAILURE;
	}

	/* pick the first one from the list */
	password_data = (GnomeKeyringNetworkPasswordData *) entries->data;

	free_password(c->password);
	c->password = xstrdup(password_data->password);

	if (!c->username)
		c->username = xstrdup(password_data->user);

	gnome_keyring_network_password_list_free(entries);

	return EXIT_SUCCESS;
}


int keyring_store(struct credential *c)
{
	guint32 item_id;
	char  *object = NULL;

	/*
	 * Sanity check that what we are storing is actually sensible.
	 * In particular, we can't make a URL without a protocol field.
	 * Without either a host or pathname (depending on the scheme),
	 * we have no primary key. And without a username and password,
	 * we are not actually storing a credential.
	 */
	if (!c->protocol || !(c->host || c->path) ||
	    !c->username || !c->password)
		return EXIT_FAILURE;

	object = keyring_object(c);

	gnome_keyring_set_network_password_sync(
				GNOME_KEYRING_DEFAULT,
				c->username,
				NULL /* domain */,
				c->host,
				object,
				c->protocol,
				NULL /* authtype */,
				c->port,
				c->password,
				&item_id);

	free(object);
	return EXIT_SUCCESS;
}

int keyring_erase(struct credential *c)
{
	char  *object = NULL;
	GList *entries;
	GnomeKeyringNetworkPasswordData *password_data;
	GnomeKeyringResult result;

	/*
	 * Sanity check that we actually have something to match
	 * against. The input we get is a restrictive pattern,
	 * so technically a blank credential means "erase everything".
	 * But it is too easy to accidentally send this, since it is equivalent
	 * to empty input. So explicitly disallow it, and require that the
	 * pattern have some actual content to match.
	 */
	if (!c->protocol && !c->host && !c->path && !c->username)
		return EXIT_FAILURE;

	object = keyring_object(c);

	result = gnome_keyring_find_network_password_sync(
				c->username,
				NULL /* domain */,
				c->host,
				object,
				c->protocol,
				NULL /* authtype */,
				c->port,
				&entries);

	free(object);

	if (result == GNOME_KEYRING_RESULT_NO_MATCH)
		return EXIT_SUCCESS;

	if (result == GNOME_KEYRING_RESULT_CANCELLED)
		return EXIT_SUCCESS;

	if (result != GNOME_KEYRING_RESULT_OK)
	{
		error("%s",gnome_keyring_result_to_message(result));
		return EXIT_FAILURE;
	}

	/* pick the first one from the list (delete all matches?) */
	password_data = (GnomeKeyringNetworkPasswordData *) entries->data;

	result = gnome_keyring_item_delete_sync(
		password_data->keyring, password_data->item_id);

	gnome_keyring_network_password_list_free(entries);

	if (result != GNOME_KEYRING_RESULT_OK)
	{
		error("%s",gnome_keyring_result_to_message(result));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

/*
 * Table with helper operation callbacks, used by generic
 * credential helper main function.
 */
struct credential_operation const credential_helper_ops[] =
{
	{ "get",   keyring_get   },
	{ "store", keyring_store },
	{ "erase", keyring_erase },
	CREDENTIAL_OP_END
};
