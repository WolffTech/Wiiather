#include "geoip.h"

#include <jansson.h>
#include <stdlib.h>
#include <string.h>

#include "https.h"

bool geoip_zip(uint32_t *zip)
{
	HttpResponse res;
	char err[128];
	if (!https_get("https://ipinfo.io/json", "application/json", &res, err, sizeof err))
		return false;
	json_t *root = res.status == 200 ? json_loadb(res.body, res.len, 0, NULL) : NULL;
	http_response_free(&res);
	if (!root)
		return false;
	const char *country = json_string_value(json_object_get(root, "country"));
	const char *postal = json_string_value(json_object_get(root, "postal"));
	bool ok = country && strcmp(country, "US") == 0 && postal && strlen(postal) == 5 &&
		strspn(postal, "0123456789") == 5;
	if (ok)
		*zip = strtoul(postal, NULL, 10);
	json_decref(root);
	return ok;
}
