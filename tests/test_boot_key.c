// SPDX-License-Identifier: Apache-2.0
#include "pet_boot_key.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    pet_boot_key_t key;
    pet_boot_key_init(&key, false, 0);
    assert(!pet_boot_key_sample(&key, true, 100));
    assert(!pet_boot_key_sample(&key, true, 129));
    assert(!pet_boot_key_sample(&key, true, 130));
    assert(pet_boot_key_busy(&key));
    assert(!pet_boot_key_sample(&key, false, 150));
    assert(!pet_boot_key_sample(&key, false, 179));
    assert(pet_boot_key_sample(&key, false, 180));
    assert(!pet_boot_key_sample(&key, false, 1000) && !pet_boot_key_busy(&key));
    // Bounce and a <30ms pulse cannot toggle; a held key never repeats.
    assert(!pet_boot_key_sample(&key, true, 1100));
    assert(!pet_boot_key_sample(&key, false, 1110));
    assert(!pet_boot_key_sample(&key, false, 1200));
    assert(!pet_boot_key_sample(&key, true, 1300));
    for (unsigned t = 1300; t < 13000; t += 20) assert(!pet_boot_key_sample(&key, true, t));
    assert(!pet_boot_key_sample(&key, false, 13000));
    assert(!pet_boot_key_sample(&key, true, 13020));
    assert(!pet_boot_key_sample(&key, false, 13030));
    assert(pet_boot_key_sample(&key, false, 13060));
    // Holding BOOT at reset/downloader handoff must not toggle on release.
    pet_boot_key_init(&key, true, 0);
    assert(!pet_boot_key_sample(&key, true, 10000));
    assert(!pet_boot_key_sample(&key, false, 10020));
    assert(!pet_boot_key_sample(&key, false, 10050));
    assert(!pet_boot_key_sample(&key, true, 10060));
    assert(!pet_boot_key_sample(&key, true, 10090));
    assert(!pet_boot_key_sample(&key, false, 10100));
    assert(pet_boot_key_sample(&key, false, 10130));
    pet_boot_key_init(&key, false, UINT32_MAX - 50);
    assert(!pet_boot_key_sample(&key, true, UINT32_MAX - 40));
    assert(!pet_boot_key_sample(&key, true, UINT32_MAX - 10));
    assert(!pet_boot_key_sample(&key, false, UINT32_MAX - 1));
    assert(pet_boot_key_sample(&key, false, 29));
    puts("PASS: BOOT debounce boundaries, press/release bounce, one event per release, long hold, startup hold suppression, timer wrap");
}
