// SPDX-License-Identifier: Apache-2.0
#include "pet_power_key.h"
bool pet_power_key_init(const pet_power_key_io_t *io)
{
    uint8_t enabled;
    if (!io->read(io->context, 0x41, &enabled)) return false;
    if (!(enabled & 0x08) && !io->write(io->context, 0x41, enabled | 0x08)) return false;
    // Discard a latched press from powering up; retain all unrelated PMIC events.
    return io->write(io->context, 0x49, 0x08);
}
int pet_power_key_poll(const pet_power_key_io_t *io)
{
    uint8_t status;
    if (!io->read(io->context, 0x49, &status)) return -1;
    if (!(status & 0x08)) return 0;
    // Do not deliver an event until the latch is successfully cleared.
    return io->write(io->context, 0x49, 0x08) ? 1 : -1;
}
