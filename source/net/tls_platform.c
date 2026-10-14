// Platform hooks that the Wii build of mbedTLS expects the app to provide
// (see scripts/build-deps.sh).
#include <gccore.h>
#include <ogc/lwp_watchdog.h>

#include <mbedtls/entropy.h>
#include <mbedtls/platform_time.h>

mbedtls_ms_time_t mbedtls_ms_time(void)
{
	return (mbedtls_ms_time_t)ticks_to_millisecs(gettime());
}

// The Wii exposes no hardware RNG to the PowerPC, so this harvests timebase
// jitter between reads separated by variable-length work. The entropy module
// conditions the output with SHA-512. This is weak entropy, acceptable only
// because the app sends no secrets: TLS here protects integrity of public
// weather data, not confidentiality.
int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{
	for (size_t i = 0; i < len; i++) {
		u8 acc = 0;
		for (int bit = 0; bit < 8; bit++) {
			u64 start = gettime();
			for (volatile u32 spin = (u32)start & 0x7f; spin; spin--)
				;
			u64 delta = gettime() - start;
			acc = (u8)((acc << 1 | acc >> 7) ^ (u8)delta ^ (u8)(start >> 3));
		}
		output[i] = acc;
	}
	*olen = len;
	return 0;
}
