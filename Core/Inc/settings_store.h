#ifndef SETTINGS_STORE_H
#define SETTINGS_STORE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define SETTINGS_PAGE_SIZE 4096U
#define SETTINGS_RECORD_SIZE 128U
/* Versioned storage payload. Keep layout fixed-width, without padding. */
typedef struct {
    uint32_t channel, frequency_mhz, mode, flags;
    uint32_t register_mask;
    uint32_t registers[11];
} ReceiverSettings;
#define SETTINGS_CHANNEL_KNOWN 1U
#define SETTINGS_MODE_KNOWN 2U
bool SettingsStore_Load(ReceiverSettings *settings, uint32_t *revision);
bool SettingsStore_Save(const ReceiverSettings *settings, uint32_t *revision);
/* Key shares the journal CRC/commit. All-FF means unprovisioned (legacy records). */
bool SettingsStore_LoadKey(uint8_t key[32]);
bool SettingsStore_SaveKey(const ReceiverSettings *settings, const uint8_t key[32], uint32_t *revision);
/* Platform backend: only the two reserved settings pages are addressable. */
bool SettingsFlash_Read(unsigned page, unsigned offset, void *data, size_t size);
bool SettingsFlash_Erase(unsigned page);
bool SettingsFlash_Program(unsigned page, unsigned offset, uint64_t data);
#endif
