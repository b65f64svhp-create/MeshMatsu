#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MESH_CONTACT_URL_MAX 256
// SharedContact URLs contain public identity only, never channel/private keys.
typedef struct {
  uint32_t number;
  char name[40], short_name[5];
  uint8_t key[32];
} mesh_contact_t;
typedef enum {
  MESH_TRUST_UNKNOWN,
  MESH_TRUST_SEEN,
  MESH_TRUST_VERIFIED,
  MESH_TRUST_CHANGED
} mesh_trust_t;
bool mesh_radio_contact(uint32_t node, mesh_contact_t *out);
bool mesh_radio_contact_url(const mesh_contact_t *contact, char *out,
                            size_t capacity);
bool mesh_radio_parse_contact_url(const char *url, mesh_contact_t *out);
mesh_trust_t mesh_radio_contact_trust(uint32_t node, const uint8_t key[32],
                                      uint8_t previous[32]);
bool mesh_radio_verify_contact(uint32_t node, const uint8_t reviewed_key[32]);
bool mesh_radio_trust_storage_ok(void);
// Security writes preserve identity, serial/managed policy and unknown fields.
typedef enum {
  MESH_MANAGE_IDLE,
  MESH_MANAGE_READING,
  MESH_MANAGE_READY,
  MESH_MANAGE_CHECKING,
  MESH_MANAGE_SAVING,
  MESH_MANAGE_VERIFYING,
  MESH_MANAGE_SAVED,
  MESH_MANAGE_REBOOT_ACK,
  MESH_MANAGE_FAILED,
  MESH_MANAGE_TIMEOUT,
  MESH_MANAGE_INTERRUPTED
} mesh_manage_state_t;
typedef struct {
  mesh_manage_state_t state;
  uint32_t node, revision, error;
  uint8_t admins[3][32], admin_count;
  bool serial_enabled, managed;
} mesh_security_t;
bool mesh_radio_security_read(void);
mesh_security_t mesh_radio_security(void);
bool mesh_radio_security_set_admin(unsigned slot, const uint8_t key[32],
                                   bool remove, uint32_t revision);
bool mesh_radio_import_contact(const mesh_contact_t *contact);
// Remote state is isolated from every local-radio cache/session.
typedef struct {
  mesh_manage_state_t state;
  uint32_t node, revision, error, role, hops;
  char name[40], short_name[5], firmware[64];
  uint8_t stage;
} mesh_remote_t;
typedef enum {
  MESH_REMOTE_OWNER,
  MESH_REMOTE_HOPS,
  MESH_REMOTE_ROLE,
  MESH_REMOTE_REBOOT
} mesh_remote_change_t;
bool mesh_radio_remote_read(uint32_t node);
mesh_remote_t mesh_radio_remote(void);
bool mesh_radio_remote_change(mesh_remote_change_t kind, const char *name,
                              const char *short_name, uint32_t value,
                              uint32_t revision);
void mesh_radio_management_clear(void);
bool mesh_contact_key_parse(const char *text, uint8_t out[32]);
