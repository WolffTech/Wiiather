#include "ca_update.h"

#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ca_bundle.h"
#include "cacert_pem.h"
#include "https.h"

// Overridable for testing against a local server.
#ifndef CA_BASE_URL
#define CA_BASE_URL "https://raw.githubusercontent.com/WolffTech/Wiiather/ca-bundle/"
#endif
#define MAX_BUNDLE (1024 * 1024)
#define MAX_SIG 160 // DER ECDSA P-256 signatures are at most 72 bytes

// Verifies cacert.pem.sig. The private key is the CA_SIGNING_KEY secret of
// the ca-bundle workflow.
static const char public_key[] = "-----BEGIN PUBLIC KEY-----\n"
				 "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE+p7XMeRJtIDLLfCnOF4LD5WACtjB\n"
				 "dHrF+KbfwK4aZNuoBroyd9z/p2MMBd/WnxLOPuLZRxewPvgt0xhYmiLJKA==\n"
				 "-----END PUBLIC KEY-----\n";

static char bundle_path[300], sig_path[300];
// The bundle in use, and the signature it was published with (none for the
// built-in one), so unchanged copies aren't downloaded again.
static uint64_t installed_date;
static unsigned char installed_sig[MAX_SIG];
static size_t installed_sig_len;

static bool signed_by_project(const void *pem, size_t len, const void *sig, size_t sig_len)
{
	unsigned char hash[32];
	if (mbedtls_sha256(pem, len, hash, 0) != 0)
		return false;
	mbedtls_pk_context pk;
	mbedtls_pk_init(&pk);
	bool ok = mbedtls_pk_parse_public_key(&pk, (const unsigned char *)public_key, sizeof public_key) == 0 &&
		  mbedtls_pk_can_do(&pk, MBEDTLS_PK_ECDSA) &&
		  mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, sizeof hash, sig, sig_len) == 0;
	mbedtls_pk_free(&pk);
	return ok;
}

// Reads a whole file of at most `max` bytes. The caller frees the result.
static char *read_file(const char *path, size_t max, size_t *len)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return NULL;
	char *data = malloc(max);
	*len = data ? fread(data, 1, max, f) : 0;
	bool whole = data && *len > 0 && *len < max && feof(f);
	fclose(f);
	if (!whole) {
		free(data);
		return NULL;
	}
	return data;
}

static bool write_file(const char *path, const void *data, size_t len)
{
	char tmp[sizeof bundle_path + 4];
	snprintf(tmp, sizeof tmp, "%s.tmp", path);
	FILE *f = fopen(tmp, "wb");
	if (!f)
		return false;
	bool ok = fwrite(data, 1, len, f) == len;
	ok = fclose(f) == 0 && ok;
	// libfat's rename won't replace an existing file.
	remove(path);
	return ok && rename(tmp, path) == 0;
}

// Installs a signed bundle at least as new as the current one. Equal dates
// only record the signature, since the certificates are the same.
static bool install(const char *pem, size_t len, const void *sig, size_t sig_len, bool *newer)
{
	uint64_t date;
	*newer = false;
	if (sig_len > MAX_SIG || !signed_by_project(pem, len, sig, sig_len) || !ca_bundle_date(pem, len, &date) ||
	    date < installed_date)
		return false;
	if (date > installed_date) {
		char err[128];
		if (!https_set_ca(pem, len, err, sizeof err))
			return false;
		*newer = true;
	}
	installed_date = date;
	memcpy(installed_sig, sig, sig_len);
	installed_sig_len = sig_len;
	return true;
}

void ca_update_init(const char *dir)
{
	snprintf(bundle_path, sizeof bundle_path, "%s/cacert.pem", dir);
	snprintf(sig_path, sizeof sig_path, "%s/cacert.pem.sig", dir);
	if (!ca_bundle_date((const char *)cacert_pem, cacert_pem_size, &installed_date))
		installed_date = 0;

	size_t len, sig_len;
	char *pem = read_file(bundle_path, MAX_BUNDLE, &len);
	char *sig = pem ? read_file(sig_path, MAX_SIG + 1, &sig_len) : NULL;
	bool newer;
	if (sig)
		install(pem, len, sig, sig_len, &newer);
	free(pem);
	free(sig);
}

uint64_t ca_update_date(void)
{
	return installed_date;
}

bool ca_update_check(bool unverified)
{
	bool (*get)(const char *, const char *, HttpResponse *, char *, size_t) =
		unverified ? https_get_unverified : https_get;
	char err[128];
	HttpResponse sig, pem = { 0 };
	if (!get(CA_BASE_URL "cacert.pem.sig", "*/*", &sig, err, sizeof err))
		return false;
	bool changed = sig.status == 200 && sig.len > 0 && sig.len <= MAX_SIG &&
		       !(sig.len == installed_sig_len && memcmp(sig.body, installed_sig, sig.len) == 0);
	bool newer = false;
	if (changed && get(CA_BASE_URL "cacert.pem", "*/*", &pem, err, sizeof err) && pem.status == 200 &&
	    pem.len < MAX_BUNDLE && install(pem.body, pem.len, sig.body, sig.len, &newer)) {
		// A failed save keeps the new bundle until the app exits.
		if (write_file(bundle_path, pem.body, pem.len))
			write_file(sig_path, sig.body, sig.len);
	}
	http_response_free(&sig);
	http_response_free(&pem);
	return newer;
}
