/*
 * A git credential helper that interface with Windows' Credential Manager
 *
 */
#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <credential_helper.h>

/* MinGW doesn't have wincred.h, so we need to define stuff */

typedef struct _CREDENTIAL_ATTRIBUTEW {
	LPWSTR Keyword;
	DWORD  Flags;
	DWORD  ValueSize;
	LPBYTE Value;
} CREDENTIAL_ATTRIBUTEW, *PCREDENTIAL_ATTRIBUTEW;

typedef struct _CREDENTIALW {
	DWORD                  Flags;
	DWORD                  Type;
	LPWSTR                 TargetName;
	LPWSTR                 Comment;
	FILETIME               LastWritten;
	DWORD                  CredentialBlobSize;
	LPBYTE                 CredentialBlob;
	DWORD                  Persist;
	DWORD                  AttributeCount;
	PCREDENTIAL_ATTRIBUTEW Attributes;
	LPWSTR                 TargetAlias;
	LPWSTR                 UserName;
} CREDENTIALW, *PCREDENTIALW;

#define CRED_TYPE_GENERIC 1
#define CRED_PERSIST_LOCAL_MACHINE 2
#define CRED_MAX_ATTRIBUTES 64

typedef BOOL (WINAPI *CredWriteWT)(PCREDENTIALW, DWORD);
typedef BOOL (WINAPI *CredUnPackAuthenticationBufferWT)(DWORD, PVOID, DWORD,
    LPWSTR, DWORD *, LPWSTR, DWORD *, LPWSTR, DWORD *);
typedef BOOL (WINAPI *CredEnumerateWT)(LPCWSTR, DWORD, DWORD *,
    PCREDENTIALW **);
typedef BOOL (WINAPI *CredPackAuthenticationBufferWT)(DWORD, LPWSTR, LPWSTR,
    PBYTE, DWORD *);
typedef VOID (WINAPI *CredFreeT)(PVOID);
typedef BOOL (WINAPI *CredDeleteWT)(LPCWSTR, DWORD, DWORD);

static HMODULE advapi, credui;
static CredWriteWT CredWriteW;
static CredUnPackAuthenticationBufferWT CredUnPackAuthenticationBufferW;
static CredEnumerateWT CredEnumerateW;
static CredPackAuthenticationBufferWT CredPackAuthenticationBufferW;
static CredFreeT CredFree;
static CredDeleteWT CredDeleteW;


static int load_cred_funcs(void)
{
	/* load DLLs */
	advapi = LoadLibrary("advapi32.dll");
	credui = LoadLibrary("credui.dll");
	if (!advapi || !credui) {
		error("failed to load DLLs");
		return EXIT_FAILURE;
	}

	/* get function pointers */
	CredWriteW = (CredWriteWT)GetProcAddress(advapi, "CredWriteW");
	CredUnPackAuthenticationBufferW = (CredUnPackAuthenticationBufferWT)
	    GetProcAddress(credui, "CredUnPackAuthenticationBufferW");
	CredEnumerateW = (CredEnumerateWT)GetProcAddress(advapi,
	    "CredEnumerateW");
	CredPackAuthenticationBufferW = (CredPackAuthenticationBufferWT)
	    GetProcAddress(credui, "CredPackAuthenticationBufferW");
	CredFree = (CredFreeT)GetProcAddress(advapi, "CredFree");
	CredDeleteW = (CredDeleteWT)GetProcAddress(advapi, "CredDeleteW");
	if (!CredWriteW || !CredUnPackAuthenticationBufferW ||
	    !CredEnumerateW || !CredPackAuthenticationBufferW || !CredFree ||
	    !CredDeleteW) {
		error("failed to load functions");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

static char target_buf[1024];
static char port_buf[8];
static WCHAR *wusername, *wpassword;

static WCHAR *utf8_to_utf16_dup(const char *str)
{
	int wlen = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
	WCHAR *wstr = xmalloc(sizeof(WCHAR) * wlen);
	if (!MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, wlen))
		die("MultiByteToWideChar failed");
	return wstr;
}

static char *utf16_to_utf8_dup(const WCHAR *wstr)
{
	int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL,
		FALSE);
	char *str = xmalloc(len);
	if (!WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, len, NULL, FALSE))
		die("WideCharToMultiByte failed");
	return str;
}

static void free_wpassword(WCHAR *pass)
{
	WCHAR *w = pass;
	if(!pass)
		return;
	while(*w) *w++ = L'\0';
	free(pass);
}

static int prepare_credential(struct credential *c)
{
	if (load_cred_funcs() )
		return EXIT_FAILURE;

	if (c->username)
		wusername = utf8_to_utf16_dup(c->username);
	if (c->password)
		wpassword = utf8_to_utf16_dup(c->password);
	if (c->port)
		snprintf(port_buf, sizeof(port_buf), "%hd", c->port);
	return EXIT_SUCCESS;
}

static int match_attr(const CREDENTIALW *cred, const WCHAR *keyword,
    const char *want)
{
	int i;
	if (!want)
		return 1;

	for (i = 0; i < cred->AttributeCount; ++i)
		if (!wcscmp(cred->Attributes[i].Keyword, keyword))
			return !strcmp((const char *)cred->Attributes[i].Value,
			    want);

	return 0; /* not found */
}

static int match_cred(const CREDENTIALW *cred, const struct credential *c)
{
	return (!wusername || !wcscmp(wusername, cred->UserName)) &&
	    match_attr(cred, L"git_protocol", c->protocol) &&
	    match_attr(cred, L"git_host", c->host) &&
	    (!c->port || match_attr(cred, L"git_port", port_buf)) &&
	    match_attr(cred, L"git_path", c->path);
}

static int get_credential(struct credential *c)
{
	WCHAR *user_buf = NULL, *pass_buf = NULL;
	DWORD user_buf_size = 0, pass_buf_size = 0;
	CREDENTIALW **creds, *cred = NULL;
	DWORD num_creds;
	int i, ret = EXIT_SUCCESS;

	if (prepare_credential(c))
		return EXIT_FAILURE;

	if (!CredEnumerateW(L"git:*", 0, &num_creds, &creds))
		return EXIT_FAILURE;

	/* search for the first credential that matches username */
	for (i = 0; i < num_creds; ++i)
		if (match_cred(creds[i], c)) {
			cred = creds[i];
			break;
		}
	if (!cred)
		goto out;

	CredUnPackAuthenticationBufferW(0, cred->CredentialBlob,
	    cred->CredentialBlobSize, NULL, &user_buf_size, NULL, NULL,
	    NULL, &pass_buf_size);

	user_buf = xmalloc(user_buf_size * sizeof(WCHAR));
	pass_buf = xmalloc(pass_buf_size * sizeof(WCHAR));

	if (!CredUnPackAuthenticationBufferW(0, cred->CredentialBlob,
	    cred->CredentialBlobSize, user_buf, &user_buf_size, NULL, NULL,
	    pass_buf, &pass_buf_size)) {
		error("CredUnPackAuthenticationBuffer failed");
		ret = EXIT_FAILURE;
		goto out;
	}

	/* zero-terminate (sizes include zero-termination) */
	user_buf[user_buf_size - 1] = L'\0';
	pass_buf[pass_buf_size - 1] = L'\0';

	if (!c->username)
		c->username = utf16_to_utf8_dup(user_buf);
	free_password(c->password);
	c->password = utf16_to_utf8_dup(pass_buf);

out:
	CredFree(creds);

	free(user_buf);
	free(wusername);
	free_wpassword(pass_buf);

	return ret;
}

static void write_attr(CREDENTIAL_ATTRIBUTEW *attr, const WCHAR *keyword,
    const char *value)
{
	attr->Keyword = (LPWSTR)keyword;
	attr->Flags = 0;
	attr->ValueSize = strlen(value) + 1; /* store zero-termination */
	attr->Value = (LPBYTE)value;
}

static int store_credential(struct credential *c)
{
	CREDENTIALW cred;
	BYTE *auth_buf;
	DWORD auth_buf_size = 0;
	CREDENTIAL_ATTRIBUTEW attrs[CRED_MAX_ATTRIBUTES];
	WCHAR *wtarget;
	int ret = EXIT_SUCCESS;

	if (prepare_credential(c))
		return EXIT_FAILURE;

	if (!wusername || !wpassword)
		return EXIT_FAILURE;

	/* prepare 'target', the unique key for the credential */
	strncat(target_buf,"git:",sizeof(target_buf));
	strncat(target_buf,c->url,sizeof(target_buf));
	wtarget = utf8_to_utf16_dup(target_buf);

	/* query buffer size */
	CredPackAuthenticationBufferW(0, wusername, wpassword,
	    NULL, &auth_buf_size);

	auth_buf = xmalloc(auth_buf_size);

	if (!CredPackAuthenticationBufferW(0, wusername, wpassword,
	    auth_buf, &auth_buf_size)) {
		error("CredPackAuthenticationBuffer failed");
		ret = EXIT_FAILURE;
		goto out;
	}

	cred.Flags = 0;
	cred.Type = CRED_TYPE_GENERIC;
	cred.TargetName = wtarget;
	cred.Comment = L"saved by git-credential-wincred";
	cred.CredentialBlobSize = auth_buf_size;
	cred.CredentialBlob = auth_buf;
	cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
	cred.AttributeCount = 1;
	cred.Attributes = attrs;
	cred.TargetAlias = NULL;
	cred.UserName = wusername;

	write_attr(attrs, L"git_protocol", c->protocol);

	if (c->host) {
		write_attr(attrs + cred.AttributeCount, L"git_host", c->host);
		cred.AttributeCount++;
	}

	if (c->port) {
		write_attr(attrs + cred.AttributeCount, L"git_port", port_buf);
		cred.AttributeCount++;
	}

	if (c->path) {
		write_attr(attrs + cred.AttributeCount, L"git_path", c->path);
		cred.AttributeCount++;
	}

	if (!CredWriteW(&cred, 0)) {
		error("CredWrite failed");
		ret = EXIT_FAILURE;
	}

out:
	free(auth_buf);
	free(wusername);
	free_wpassword(wpassword);
	free(wtarget);
	return ret;
}

static int erase_credential(struct credential *c)
{
	CREDENTIALW **creds;
	DWORD num_creds;
	int i;

	if (prepare_credential(c))
		return EXIT_FAILURE;

	if (!CredEnumerateW(L"git:*", 0, &num_creds, &creds))
		return EXIT_FAILURE;

	for (i = 0; i < num_creds; ++i) {
		if (match_cred(creds[i],c))
			CredDeleteW(creds[i]->TargetName, creds[i]->Type, 0);
	}

	CredFree(creds);
	return EXIT_SUCCESS;
}

/*
 * Table with helper operation callbacks, used by generic
 * credential helper main function.
 */
struct credential_operation const credential_helper_ops[] =
{
	{ "get",   get_credential   },
	{ "store", store_credential },
	{ "erase", erase_credential },
	CREDENTIAL_OP_END
};
