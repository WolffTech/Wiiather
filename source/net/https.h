#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef struct {
	int status;
	char *body; // NUL-terminated; owned by the response
	size_t len;
} HttpResponse;

// Loads the CA bundle and seeds the DRBG. Call once after the network is up.
bool https_init(char *err, size_t err_len);

// Blocking GET that follows redirects. Not thread-safe: call from one thread.
// On success the caller must free the response with http_response_free().
bool https_get(const char *url, const char *accept, HttpResponse *out, char *err, size_t err_len);

void http_response_free(HttpResponse *res);
