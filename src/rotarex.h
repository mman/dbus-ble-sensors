#ifndef ROTAREX_H
#define ROTAREX_H

#include <stdint.h>
#include <bluetooth/bluetooth.h>

int rotarex_handle_mfg(const bdaddr_t *addr, const uint8_t *buf, int len);

#endif
