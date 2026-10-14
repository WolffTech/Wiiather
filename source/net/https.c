#include "https.h"

#include <errno.h>
#include <network.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>

#include "cacert_pem.h"
#include "util/utc.h"

#define USER_AGENT "Wiiather/0.1 (Wii homebrew; WeatherStar 4000 simulator)"
#define IO_TIMEOUT_MS 15000
#define MAX_RESPONSE (4 * 1024 * 1024)
#define MAX_REDIRECTS 3

static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context drbg;
static mbedtls_x509_crt ca_chain;
static bool initialized;
static time_t last_date, last_date_rtc;

static void set_err(char *err, size_t err_len, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(err, err_len, fmt, ap);
	va_end(ap);
}

static void set_tls_err(char *err, size_t err_len, const char *what, int ret)
{
	char buf[96];
	mbedtls_strerror(ret, buf, sizeof buf);
	set_err(err, err_len, "%s: %s (-0x%04x)", what, buf, (unsigned)-ret);
}

bool https_init(char *err, size_t err_len)
{
	if (initialized)
		return true;

	psa_status_t status = psa_crypto_init();
	if (status != PSA_SUCCESS) {
		set_err(err, err_len, "psa_crypto_init failed (%d)", (int)status);
		return false;
	}

	mbedtls_entropy_init(&entropy);
	mbedtls_ctr_drbg_init(&drbg);
	static const char pers[] = "wiiather";
	int ret = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy,
		(const unsigned char *)pers, sizeof pers - 1);
	if (ret != 0) {
		set_tls_err(err, err_len, "DRBG seed", ret);
		return false;
	}

	// mbedTLS needs the PEM NUL-terminated, and bin2o data is not.
	char *pem = malloc(cacert_pem_size + 1);
	if (!pem) {
		set_err(err, err_len, "out of memory loading CA bundle");
		return false;
	}
	memcpy(pem, cacert_pem, cacert_pem_size);
	pem[cacert_pem_size] = '\0';
	mbedtls_x509_crt_init(&ca_chain);
	ret = mbedtls_x509_crt_parse(&ca_chain, (const unsigned char *)pem, cacert_pem_size + 1);
	free(pem);
	// A positive return means some certificates were skipped, which is fine.
	if (ret < 0) {
		set_tls_err(err, err_len, "CA bundle", ret);
		return false;
	}

	initialized = true;
	return true;
}

typedef struct {
	char host[128];
	char path[512];
	unsigned short port;
} Url;

static bool parse_url(const char *url, Url *out)
{
	static const char scheme[] = "https://";
	if (strncasecmp(url, scheme, sizeof scheme - 1) != 0)
		return false;
	const char *host = url + sizeof scheme - 1;
	size_t host_len = strcspn(host, ":/?");
	if (host_len == 0 || host_len >= sizeof out->host)
		return false;
	memcpy(out->host, host, host_len);
	out->host[host_len] = '\0';

	const char *rest = host + host_len;
	out->port = 443;
	if (*rest == ':') {
		out->port = (unsigned short)strtoul(rest + 1, (char **)&rest, 10);
	}
	if (*rest == '\0')
		rest = "/";
	if (*rest == '?') {
		if (snprintf(out->path, sizeof out->path, "/%s", rest) >= (int)sizeof out->path)
			return false;
	} else if (snprintf(out->path, sizeof out->path, "%s", rest) >= (int)sizeof out->path) {
		return false;
	}
	return true;
}

static int bio_send(void *ctx, const unsigned char *buf, size_t len)
{
	s32 sock = *(s32 *)ctx;
	s32 ret = net_send(sock, buf, (s32)len, 0);
	if (ret == -EAGAIN)
		return MBEDTLS_ERR_SSL_WANT_WRITE;
	if (ret < 0)
		return MBEDTLS_ERR_NET_SEND_FAILED;
	return ret;
}

static int bio_recv_timeout(void *ctx, unsigned char *buf, size_t len, uint32_t timeout_ms)
{
	s32 sock = *(s32 *)ctx;
	struct pollsd pfd = { .socket = sock, .events = POLLIN };
	s32 ready = net_poll(&pfd, 1, timeout_ms ? (s32)timeout_ms : -1);
	if (ready == 0)
		return MBEDTLS_ERR_SSL_TIMEOUT;
	if (ready < 0)
		return MBEDTLS_ERR_NET_RECV_FAILED;
	s32 ret = net_recv(sock, buf, (s32)len, 0);
	if (ret == -EAGAIN)
		return MBEDTLS_ERR_SSL_WANT_READ;
	if (ret < 0)
		return MBEDTLS_ERR_NET_RECV_FAILED;
	return ret;
}

static s32 tcp_connect(const Url *url, char *err, size_t err_len)
{
	struct hostent *he = net_gethostbyname(url->host);
	if (!he || he->h_length != 4 || !he->h_addr_list[0]) {
		set_err(err, err_len, "DNS lookup failed for %s", url->host);
		return -1;
	}
	s32 sock = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (sock < 0) {
		set_err(err, err_len, "socket() failed (%d)", sock);
		return -1;
	}
	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_port = htons(url->port);
	memcpy(&addr.sin_addr, he->h_addr_list[0], 4);
	s32 ret = net_connect(sock, (struct sockaddr *)&addr, sizeof addr);
	if (ret < 0) {
		net_close(sock);
		set_err(err, err_len, "connect to %s failed (%d)", url->host, ret);
		return -1;
	}
	return sock;
}

// Growable buffer for the raw response.
typedef struct {
	char *data;
	size_t len, cap;
} Buffer;

static bool buffer_reserve(Buffer *b, size_t extra)
{
	if (b->len + extra + 1 <= b->cap)
		return true;
	size_t cap = b->cap ? b->cap : 16384;
	while (cap < b->len + extra + 1)
		cap *= 2;
	char *data = realloc(b->data, cap);
	if (!data)
		return false;
	b->data = data;
	b->cap = cap;
	return true;
}

static bool tls_exchange(const Url *url, const char *accept, Buffer *raw, char *err, size_t err_len)
{
	s32 sock = tcp_connect(url, err, err_len);
	if (sock < 0)
		return false;

	mbedtls_ssl_context ssl;
	mbedtls_ssl_config conf;
	mbedtls_ssl_init(&ssl);
	mbedtls_ssl_config_init(&conf);
	bool ok = false;
	int ret;

	if ((ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
		     MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
		set_tls_err(err, err_len, "TLS config", ret);
		goto done;
	}
	mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
	mbedtls_ssl_conf_ca_chain(&conf, &ca_chain, NULL);
	mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &drbg);
	mbedtls_ssl_conf_read_timeout(&conf, IO_TIMEOUT_MS);

	if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0 || (ret = mbedtls_ssl_set_hostname(&ssl, url->host)) != 0) {
		set_tls_err(err, err_len, "TLS setup", ret);
		goto done;
	}
	mbedtls_ssl_set_bio(&ssl, &sock, bio_send, NULL, bio_recv_timeout);

	while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
		if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
			continue;
		uint32_t flags = mbedtls_ssl_get_verify_result(&ssl);
		if (flags & (MBEDTLS_X509_BADCERT_EXPIRED | MBEDTLS_X509_BADCERT_FUTURE))
			set_err(err, err_len, "%s certificate date invalid. Check the Wii clock.", url->host);
		else
			set_tls_err(err, err_len, "TLS handshake", ret);
		goto done;
	}

	char request[1024];
	int req_len = snprintf(request, sizeof request,
		"GET %s HTTP/1.1\r\n"
		"Host: %s\r\n"
		"User-Agent: " USER_AGENT "\r\n"
		"Accept: %s\r\n"
		"Accept-Encoding: identity\r\n"
		"Connection: close\r\n\r\n",
		url->path, url->host, accept);
	if (req_len >= (int)sizeof request) {
		set_err(err, err_len, "request too long");
		goto done;
	}
	for (int sent = 0; sent < req_len;) {
		ret = mbedtls_ssl_write(&ssl, (const unsigned char *)request + sent, req_len - sent);
		if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
			continue;
		if (ret < 0) {
			set_tls_err(err, err_len, "TLS write", ret);
			goto done;
		}
		sent += ret;
	}

	for (;;) {
		if (!buffer_reserve(raw, 16384)) {
			set_err(err, err_len, "out of memory");
			goto done;
		}
		ret = mbedtls_ssl_read(&ssl, (unsigned char *)raw->data + raw->len, raw->cap - raw->len - 1);
		if (ret > 0) {
			raw->len += ret;
			if (raw->len > MAX_RESPONSE) {
				set_err(err, err_len, "response too large");
				goto done;
			}
			continue;
		}
		if (ret == 0 || ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
			break;
		if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE ||
		    ret == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET)
			continue;
		set_tls_err(err, err_len, "TLS read", ret);
		goto done;
	}
	raw->data[raw->len] = '\0';
	mbedtls_ssl_close_notify(&ssl);
	ok = true;

done:
	mbedtls_ssl_free(&ssl);
	mbedtls_ssl_config_free(&conf);
	net_close(sock);
	return ok;
}

// Finds a header value in the header block. Returns its length, or -1.
static int find_header(const char *headers, const char *name, const char **value)
{
	size_t name_len = strlen(name);
	for (const char *line = strstr(headers, "\r\n"); line && line[2] != '\r'; line = strstr(line + 2, "\r\n")) {
		const char *h = line + 2;
		if (strncasecmp(h, name, name_len) == 0 && h[name_len] == ':') {
			const char *v = h + name_len + 1;
			while (*v == ' ' || *v == '\t')
				v++;
			*value = v;
			return (int)strcspn(v, "\r");
		}
	}
	return -1;
}

// Decodes a chunked body in place. Returns the decoded length, or -1.
static long dechunk(char *body, size_t len)
{
	char *src = body, *dst = body, *end = body + len;
	for (;;) {
		char *line_end;
		unsigned long size = strtoul(src, &line_end, 16);
		line_end = strstr(line_end, "\r\n");
		if (!line_end || line_end + 2 > end)
			return -1;
		src = line_end + 2;
		if (size == 0)
			break;
		if (src + size > end)
			return -1;
		memmove(dst, src, size);
		dst += size;
		src += size + 2; // skip trailing CRLF
		if (src > end)
			return -1;
	}
	*dst = '\0';
	return dst - body;
}

static bool parse_response(Buffer *raw, HttpResponse *out, char *location, size_t location_len, char *err,
	size_t err_len)
{
	char *body = strstr(raw->data, "\r\n\r\n");
	if (!body || sscanf(raw->data, "HTTP/%*s %d", &out->status) != 1) {
		set_err(err, err_len, "malformed HTTP response");
		return false;
	}
	body[2] = '\0'; // terminate the header block after the last header's CRLF
	const char *headers = raw->data;
	body += 4;
	size_t body_len = raw->len - (body - raw->data);

	const char *value;
	int value_len;
	location[0] = '\0';
	if ((value_len = find_header(headers, "Location", &value)) > 0 && (size_t)value_len < location_len) {
		memcpy(location, value, value_len);
		location[value_len] = '\0';
	}
	char date[40];
	if ((value_len = find_header(headers, "Date", &value)) > 0 && (size_t)value_len < sizeof date) {
		memcpy(date, value, value_len);
		date[value_len] = '\0';
		if (!utc_parse_http_date(date, &out->date))
			out->date = 0;
	}
	if (out->date) {
		last_date = out->date;
		last_date_rtc = time(NULL);
	}
	if ((value_len = find_header(headers, "Transfer-Encoding", &value)) > 0 &&
	    strncasecmp(value, "chunked", 7) == 0) {
		long decoded = dechunk(body, body_len);
		if (decoded < 0) {
			set_err(err, err_len, "malformed chunked body");
			return false;
		}
		body_len = decoded;
	}

	out->len = body_len;
	out->body = malloc(body_len + 1);
	if (!out->body) {
		set_err(err, err_len, "out of memory");
		return false;
	}
	memcpy(out->body, body, body_len);
	out->body[body_len] = '\0';
	return true;
}

bool https_get(const char *url_str, const char *accept, HttpResponse *out, char *err, size_t err_len)
{
	*out = (HttpResponse){ 0 };
	if (!initialized) {
		set_err(err, err_len, "https_init not called");
		return false;
	}

	char current[640];
	snprintf(current, sizeof current, "%s", url_str);
	for (int hop = 0; hop <= MAX_REDIRECTS; hop++) {
		Url url;
		if (!parse_url(current, &url)) {
			set_err(err, err_len, "unsupported URL: %.60s", current);
			return false;
		}
		Buffer raw = { 0 };
		char location[512];
		bool ok = tls_exchange(&url, accept, &raw, err, err_len) &&
			  parse_response(&raw, out, location, sizeof location, err, err_len);
		free(raw.data);
		if (!ok)
			return false;

		bool redirect = out->status >= 300 && out->status < 400 && location[0];
		if (!redirect)
			return true;
		http_response_free(out);
		if (location[0] == '/')
			snprintf(current, sizeof current, "https://%s%s", url.host, location);
		else
			snprintf(current, sizeof current, "%s", location);
	}
	set_err(err, err_len, "too many redirects");
	return false;
}

void http_response_free(HttpResponse *res)
{
	free(res->body);
	*res = (HttpResponse){ 0 };
}

bool https_last_date(time_t *utc, time_t *rtc)
{
	*utc = last_date;
	*rtc = last_date_rtc;
	return last_date != 0;
}
