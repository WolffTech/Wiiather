# Wiiather CA bundle

Published automatically by `scripts/publish-ca-bundle.sh` on `main`.

- `cacert.pem`: curl's extract of Mozilla's CA certificates
  (https://curl.se/docs/caextract.html), unmodified, under the MPL 2.0.
- `cacert.pem.sig`: ECDSA P-256 signature of its SHA-256, made with the
  project's key. Wiiather installs newer bundles only if this verifies.
