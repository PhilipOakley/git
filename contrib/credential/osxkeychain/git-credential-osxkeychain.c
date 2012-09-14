
#include <credential_helper.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <Security/Security.h>

static SecProtocolType protocol;

#define KEYCHAIN_ITEM(x) (x ? strlen(x) : 0), x
#define KEYCHAIN_ARGS(c) \
	NULL, /* default keychain */ \
	KEYCHAIN_ITEM(c->host), \
	0, NULL, /* account domain */ \
	KEYCHAIN_ITEM(c->username), \
	KEYCHAIN_ITEM(c->path), \
	(UInt16) c->port, \
	protocol, \
	kSecAuthenticationTypeDefault

static int prepare_internet_password(struct credential *c)
{
	if (!c->protocol)
		return -1;
	else if (!strcmp(c->protocol, "https"))
		protocol = kSecProtocolTypeHTTPS;
	else if (!strcmp(c->protocol, "http"))
		protocol = kSecProtocolTypeHTTP;
	else /* we don't yet handle other protocols */
		return -1;

	return 0;
}

static void
find_username_in_item(SecKeychainItemRef item, struct credential *c)
{
	SecKeychainAttributeList list;
	SecKeychainAttribute attr;

	list.count = 1;
	list.attr = &attr;
	attr.tag = kSecAccountItemAttr;

	if (SecKeychainItemCopyContent(item, NULL, &list, NULL, NULL))
		return;

	free(c->username);
	c->username = xstrndup(attr.data, attr.length);

	SecKeychainItemFreeContent(&list, NULL);
}

static int find_internet_password(struct credential *c)
{
	void *buf;
	UInt32 len;
	SecKeychainItemRef item;

	/* Silently ignore unsupported protocols */
	if (prepare_internet_password(c))
		return EXIT_SUCCESS;

	if (SecKeychainFindInternetPassword(KEYCHAIN_ARGS(c), &len, &buf, &item))
		return EXIT_SUCCESS;

	free_password(c->password);
	c->password = xstrndup(buf, len);
	memset(buf,len,'\0');

	if (!c->username)
		find_username_in_item(item, c);

	SecKeychainItemFreeContent(NULL, buf);
	return EXIT_SUCCESS;
}

static int delete_internet_password(struct credential *c)
{
	SecKeychainItemRef item;

	/*
	 * Require at least a protocol and host for removal, which is what git
	 * will give us; if you want to do something more fancy, use the
	 * Keychain manager.
	 */
	if (!c->protocol || !c->host)
		return EXIT_FAILURE;

	/* Silently ignore unsupported protocols */
	if (prepare_internet_password(c))
		return EXIT_SUCCESS;

	if (SecKeychainFindInternetPassword(KEYCHAIN_ARGS(c), 0, NULL, &item))
		return EXIT_SUCCESS;

	if (!SecKeychainItemDelete(item))
		return EXIT_SUCCESS;

	return EXIT_FAILURE;
}

static int add_internet_password(struct credential *c)
{
	/* Only store complete credentials */
	if (!c->protocol || !c->host || !c->username || !c->password)
		return EXIT_FAILURE;

	if (prepare_internet_password(c))
		return EXIT_FAILURE;

	if (SecKeychainAddInternetPassword(
	      KEYCHAIN_ARGS(c),
	      KEYCHAIN_ITEM(c->password),
	      NULL))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

/*
 * Table with helper operation callbacks, used by generic
 * credential helper main function.
 */
struct credential_operation const credential_helper_ops[] =
{
	{ "get",   find_internet_password   },
	{ "store", add_internet_password    },
	{ "erase", delete_internet_password },
	CREDENTIAL_OP_END
};
