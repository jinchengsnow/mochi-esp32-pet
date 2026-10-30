// SPDX-License-Identifier: Apache-2.0
#include "pet_power_key.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint8_t reg[256]; bool fail_read, fail_write; unsigned writes; uint8_t last_reg, last_value; } fixture_t;
static bool read_reg(void *context, uint8_t reg, uint8_t *value)
{
    fixture_t *f = context;
    if (f->fail_read) return false;
    *value = f->reg[reg];
    return true;
}
static bool write_reg(void *context, uint8_t reg, uint8_t value)
{
    fixture_t *f = context;
    if (f->fail_write) return false;
    f->last_reg = reg; f->last_value = value; ++f->writes;
    if (reg == 0x49) f->reg[reg] &= (uint8_t)~value; // PMIC write-one-to-clear semantics.
    else f->reg[reg] = value;
    return true;
}
int main(void)
{
    fixture_t f = {0};
    pet_power_key_io_t io = {.context = &f, .read = read_reg, .write = write_reg};
    f.reg[0x41] = 0xa4; // Keep VBUS/battery/long-press enable bits untouched.
    f.reg[0x49] = 0x9c; // Stale short + unrelated VBUS/battery/long flags.
    assert(pet_power_key_init(&io));
    assert(f.reg[0x41] == 0xac && f.reg[0x49] == 0x94);
    assert(f.last_reg == 0x49 && f.last_value == 0x08);
    assert(pet_power_key_poll(&io) == 0); // No phantom toggle at boot.
    unsigned writes = f.writes;
    f.reg[0x49] = 0x04; // Long press is not a screen toggle.
    assert(pet_power_key_poll(&io) == 0 && f.writes == writes);
    f.reg[0x49] = 0x9c;
    assert(pet_power_key_poll(&io) == 1 && f.reg[0x49] == 0x94);
    assert(pet_power_key_poll(&io) == 0); // Same latched event is delivered once.
    f.reg[0x49] |= 0x08;
    f.fail_write = true;
    assert(pet_power_key_poll(&io) == -1 && f.reg[0x49] == 0x9c);
    f.fail_write = false;
    assert(pet_power_key_poll(&io) == 1 && pet_power_key_poll(&io) == 0);
    f.fail_read = true;
    assert(!pet_power_key_init(&io) && pet_power_key_poll(&io) == -1);
    puts("PASS: PWR short-press enable/preserve, stale boot clear, W1C selective ack, long ignored, once-only, I/O retry");
}
