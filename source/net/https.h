#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

typedef struct {
	int status;
	char *body; // NUL-terminated; owned by the response
	size_t len;
	time_t date; // server's Date header in UTC, 0 if absent
} HttpResponse;

// Loads the CA bundle and seeds the DRBG. Call once after the network is up.
bool https_init(char *err, size_t err_len);

// Blocking GET that follows redirects. Not thread-safe: call from one thread.
// On success the caller must free the response with http_response_free().
bool https_get(const char *url, const char *accept, HttpResponse *out, char *err, size_t err_len);

// Same, without checking the server's certificate. Only for content that is
// verified another way, such as the signed CA bundle.
bool https_get_unverified(const char *url, const char *accept, HttpResponse *out, char *err, size_t err_len);

void http_response_free(HttpResponse *res);

// Replaces the trusted CA certificates. Keeps the current ones on failure.
bool https_set_ca(const void *pem, size_t len, char *err, size_t err_len);

// Whether a request failed because no trusted CA vouched for the server
// since the last call. Clears the flag.
bool https_take_trust_failure(void);

// The latest response's Date header in UTC and the RTC reading when it was
// parsed, for deriving the RTC's UTC offset. False until a response has one.
bool https_last_date(time_t *utc, time_t *rtc);
