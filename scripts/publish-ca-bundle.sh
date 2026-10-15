#!/usr/bin/env bash
# Publishes curl's extract of Mozilla's CA bundle, signed with the project's
# key, to the ca-bundle branch, where the app checks for updates (see
# source/net/ca_update.c). Does nothing unless curl's bundle is newer than
# the published one.
#
# Needs CA_SIGNING_KEY (the PEM private key) and a checkout whose `origin`
# can be pushed to. Run from the repository root.
set -euo pipefail

: "${CA_SIGNING_KEY:?set CA_SIGNING_KEY to the PEM private key}"
: "${CA_SOURCE_URL:=https://curl.se/ca/cacert.pem}"
BRANCH=ca-bundle
MIN_CERTS=100

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# The "as of" date as YYYYMMDDhhmmss, like ca_bundle_date() in the app.
bundle_date() {
	python3 -I -c '
import sys, time
for line in open(sys.argv[1], encoding="utf-8"):
    if line.startswith("## Certificate data from Mozilla as of: "):
        when = time.strptime(line.split(": ", 1)[1].strip(), "%a %b %d %H:%M:%S %Y GMT")
        print(time.strftime("%Y%m%d%H%M%S", when))
        break
else:
    sys.exit("no Mozilla date in " + sys.argv[1])
' "$1"
}

curl -fsSL -o "$WORK/cacert.pem" "$CA_SOURCE_URL"
curl -fsSL -o "$WORK/cacert.pem.sha256" "$CA_SOURCE_URL.sha256"
(cd "$WORK" && sha256sum -c cacert.pem.sha256)
certs="$(grep -c 'BEGIN CERTIFICATE' "$WORK/cacert.pem")"
if [ "$certs" -lt "$MIN_CERTS" ]; then
	echo "Only $certs certificates in the download; refusing to publish" >&2
	exit 1
fi
new_date="$(bundle_date "$WORK/cacert.pem")"

published_date=0
parent=()
if git fetch -q --depth 1 origin "$BRANCH" 2>/dev/null; then
	git show FETCH_HEAD:cacert.pem > "$WORK/published.pem"
	published_date="$(bundle_date "$WORK/published.pem")"
	parent=(-p "$(git rev-parse FETCH_HEAD)")
fi
if [ "$new_date" -le "$published_date" ]; then
	echo "Published bundle ($published_date) is current; curl has $new_date"
	exit 0
fi

# Sign, and refuse a key that doesn't match the public key built into the app.
(umask 077 && printf '%s\n' "$CA_SIGNING_KEY" > "$WORK/key.pem")
openssl ec -in "$WORK/key.pem" -pubout -out "$WORK/pub.pem" 2>/dev/null
built_in="$(grep -o '"[A-Za-z0-9+/=]*\\n"' source/net/ca_update.c | tr -d '"' | sed 's/\\n$//' | tr -d '\n')"
if [ "$(grep -v -- '-----' "$WORK/pub.pem" | tr -d '\n')" != "$built_in" ]; then
	echo "CA_SIGNING_KEY doesn't match the public key in source/net/ca_update.c" >&2
	exit 1
fi
openssl dgst -sha256 -sign "$WORK/key.pem" -out "$WORK/cacert.pem.sig" "$WORK/cacert.pem"
openssl dgst -sha256 -verify "$WORK/pub.pem" -signature "$WORK/cacert.pem.sig" "$WORK/cacert.pem"

# One commit per bundle on a branch that shares no history with main, built
# with plumbing so the checkout's credentials are reused and its working
# tree is left alone.
cat > "$WORK/README.md" <<'EOF'
# Wiiather CA bundle

Published automatically by `scripts/publish-ca-bundle.sh` on `main`.

- `cacert.pem`: curl's extract of Mozilla's CA certificates
  (https://curl.se/docs/caextract.html), unmodified, under the MPL 2.0.
- `cacert.pem.sig`: ECDSA P-256 signature of its SHA-256, made with the
  project's key. Wiiather installs newer bundles only if this verifies.
EOF
tree="$(for f in README.md cacert.pem cacert.pem.sig; do
	printf '100644 blob %s\t%s\n' "$(git hash-object -w "$WORK/$f")" "$f"
done | git mktree)"
export GIT_AUTHOR_NAME="github-actions[bot]" GIT_COMMITTER_NAME="github-actions[bot]"
export GIT_AUTHOR_EMAIL="41898282+github-actions[bot]@users.noreply.github.com"
export GIT_COMMITTER_EMAIL="$GIT_AUTHOR_EMAIL"
commit="$(git commit-tree "${parent[@]}" -m "Update CA bundle to $new_date" "$tree")"
git push -q origin "$commit:refs/heads/$BRANCH"
echo "Published CA bundle $new_date"
