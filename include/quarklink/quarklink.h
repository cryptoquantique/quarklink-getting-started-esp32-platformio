/**
 * \file quarklink.h
 * \brief Where the QuarkLink magic happens.
 * \copyright Copyright (c) 2026 Crypto Quantique Ltd. All rights reserved.
 */
#ifndef QUARKLINK_H
#define QUARKLINK_H

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Single entrypoint for build-time configuration + validation */
#include "quarklink_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Human-readable library version string. */
extern const char quarklink_version[];

/** Size of \c device_id: 64 hex-encoded bytes + NUL. */
#define QUARKLINK_DEVICE_ID_LENGTH (65U)

/** Maximum size of endpoint hostname buffers (QuarkLink instance and IoT Hub), including NUL. */
#define QUARKLINK_MAX_ENDPOINT_LENGTH (128U)

/** Maximum size of PEM private-key export buffers (\ref quarklink_export_device_key_pem). */
#define QUARKLINK_MAX_KEY_LENGTH (256U)

/** Maximum size of PEM certificate buffers in \ref quarklink_context_t (\c device_cert, \c iot_hub_root_cert). */
#define QUARKLINK_MAX_CERT_LENGTH (2048U)

/**
 * \brief Quarklink return codes.
 *
 * Negative values are errors, \c QUARKLINK_SUCCESS is success, and positive values
 * (below) are non-error outcomes for firmware update, enrolment, and persistence.
 */
typedef enum {
    QUARKLINK_ERROR = -1,                        /*!< Unspecified failure. */
    QUARKLINK_INVALID_PARAMETER = -2,            /*!< NULL pointer or out-of-range argument. */
    QUARKLINK_COMMUNICATION_ERROR = -3,          /*!< Transport or TLS failure. */
    QUARKLINK_VALUE_NOT_AVAILABLE = -4,          /*!< Requested value not present in context. */
    QUARKLINK_NVM_ERROR = -5,                    /*!< Non-volatile storage read/write failure. */
    QUARKLINK_NOT_INITIALISED = -6,              /*!< Context not initialised. */
    QUARKLINK_AZURE_PROVISIONING_ERROR = -7,     /*!< Azure DPS provisioning failed. */
    QUARKLINK_CONNECTION_LOST = -8,              /*!< Connection dropped during operation. */
    QUARKLINK_FW_UPDATE_ERROR = -9,              /*!< Firmware update failed for other reasons. */
    QUARKLINK_FW_UPDATE_INVALID_SIGNATURE = -10, /*!< Firmware signature verification failed. */
    QUARKLINK_FW_UPDATE_MISSING_SIGNATURE = -11, /*!< Firmware signature required but absent. */
    QUARKLINK_SUCCESS = 0,                       /*!< Operation completed successfully. */
    QUARKLINK_FW_UPDATE_NO_UPDATE = 1,           /*!< No firmware update available. */
    QUARKLINK_FW_UPDATE_UPDATED = 2,             /*!< Firmware updated successfully. */
    QUARKLINK_DEVICE_DOES_NOT_EXIST = 3,         /*!< Device unknown to QuarkLink. */
    QUARKLINK_DEVICE_REVOKED = 4,                /*!< Device revoked on QuarkLink. */
    QUARKLINK_CONTEXT_NO_CREDENTIALS_STORED = 5, /*!< NVM has no instance config. */
    QUARKLINK_CONTEXT_NO_ENROLMENT_INFO_STORED = 6, /*!< NVM has instance but no enrolment. */
    QUARKLINK_CONTEXT_NOTHING_STORED = 7,           /*!< NVM has no stored context. */
} quarklink_return_t;

/**
 * Byte size of \ref quarklink_internal_state_t for static allocation.
 *
 * Fixed across builds so callers can budget RAM; the opaque layout may differ by
 * \c QL_CFG_KEYPROV but the published size is always large enough.
 */
#if QL_KEYPROV_IS_SOFTWARE(QL_CFG_KEYPROV)
#define QUARKLINK_INTERNAL_STATE_SIZE (7480U)
#else
#define QUARKLINK_INTERNAL_STATE_SIZE (7200U)
#endif

/**
 * \brief Opaque internal state (caller-allocated, library-owned).
 *
 * Layout is hidden; use \c sizeof(quarklink_internal_state_t) or \c QUARKLINK_INTERNAL_STATE_SIZE
 * for RAM budgeting. Do not read or write \c _storage — only pass the object to \ref quarklink_init().
 */
typedef struct quarklink_internal_state {
    alignas(max_align_t) uint8_t
        _storage[QUARKLINK_INTERNAL_STATE_SIZE]; /**< Opaque storage; do not read or write. */
} quarklink_internal_state_t;

/**
 * \brief Quarklink context.
 *
 * Long-lived fields the application may read after init / enrol / status.
 * Allocate separately: \c sizeof(quarklink_context_t) plus \c sizeof(quarklink_internal_state_t).
 *
 * QuarkLink instance connection settings (hostname, root cert, port) are library-owned in
 * internal state. Use \ref quarklink_get_endpoint() to read the current instance hostname . 
 * To change instance details, re-initialise the context using \ref quarklink_init() again.
 *
 * All certificates are in PEM format.
 * \note Define both \ref quarklink_context_t and \ref quarklink_internal_state_t, then call \ref quarklink_init.
 * After quarklink_init: device_id.
 * After quarklink_enrol: device_cert, iot_hub_root_cert, iot_hub_endpoint, iot_hub_port.
 * After quarklink_status: use the \c quarklink_is_device_*() helpers (see below).
 *
 * \par IoT Hub connectivity (MQTTS)
 * After \ref quarklink_enrol(), read \c device_cert, \c iot_hub_root_cert,
 * \c iot_hub_endpoint, and \c iot_hub_port from this structure for broker mTLS.
 * For the client private key, include the supplement header matching \c QL_CFG_KEYPROV:
 * - Software (\c QL_KEYPROV_MBEDTLS / \c QL_KEYPROV_UECC): \c quarklink/keyprov/quarklink_software_key.h
 * - ESP32 DS: \c quarklink/esp32idf/quarklink_esp32idf_ds.h
 * - ESP32 ATECC608: \c quarklink/esp32idf/quarklink_esp32idf_atecc608.h
 */
typedef struct quarklink_context_t {

    /** Unique Device ID, initialised by \ref quarklink_init() */
    char device_id[QUARKLINK_DEVICE_ID_LENGTH];
    /** Device certificate PEM, obtained by \ref quarklink_enrol(). */
    char device_cert[QUARKLINK_MAX_CERT_LENGTH];

    /** IoT Hub root CA certificate PEM, obtained by \ref quarklink_enrol(). */
    char iot_hub_root_cert[QUARKLINK_MAX_CERT_LENGTH];
    /** IoT Hub broker hostname, obtained by \ref quarklink_enrol(). */
    char iot_hub_endpoint[QUARKLINK_MAX_ENDPOINT_LENGTH];
    /** IoT Hub broker port (typically 8883 for MQTTS), obtained by \ref quarklink_enrol(). */
    uint16_t iot_hub_port;
    /** Azure DPS scope ID when using Azure Central; NULL otherwise. Library-owned pointer. */
    char *scope_id;

    /** MQTT topic for firmware-update notifications; NULL if not provided. Library-owned pointer. */
    char *fw_update_topic;

    /**
     * Per-instance internal state (library-owned; do not dereference or modify).
     * Set by \ref quarklink_init(); must remain valid for the lifetime of this context.
     */
    quarklink_internal_state_t *internal;

} quarklink_context_t;

/**
 * \brief     	Initialise the Quarklink environment.
 *
 * \note 		This function needs to be called before any other QuarkLink API.
 * \param[in,out] ctx the QuarkLink context to initialise. \note This structure is erased before initialisation.
 * \param[in,out] state     caller-allocated internal state (\c sizeof(quarklink_internal_state_t)); erased on init.
 * \param[in] 	endpoint    QuarkLink instance hostname (e.g. \c cryptoquantique.quarklink.io);
 *                              must fit in \ref QUARKLINK_MAX_ENDPOINT_LENGTH bytes.
 * \param[in] 	root_cert   PEM root certificate for the QuarkLink instance TLS trust anchor.
 *                              Must be a single PEM certificate under 1500 bytes (including NUL);
 *                              this is smaller than \ref QUARKLINK_MAX_CERT_LENGTH because the
 *                              instance CA is stored in internal state, not in the public context.
 * \retval      QUARKLINK_SUCCESS for success
 * \retval      Others for error
 */
quarklink_return_t quarklink_init(quarklink_context_t *ctx,
                                  quarklink_internal_state_t *state,
                                  const char *endpoint,
                                  const char *root_cert);

/**
 * \brief     	Enrol with Quarklink to provision and get credentials.
 * \note 		None.
 * \param[in,out] ctx the QuarkLink context, make sure to initialise it with \ref quarklink_init() first
 * \retval      QUARKLINK_SUCCESS for success
 * \retval      QUARKLINK_DEVICE_DOES_NOT_EXIST
 * \retval      QUARKLINK_DEVICE_REVOKED
 * \retval      QUARKLINK_COMMUNICATION_ERROR Transport or TLS failure (e.g. unreachable instance)
 * \retval      QUARKLINK_ERROR Enrolment failed (server rejection, unrecognised HTTP status, parse error, etc.)
 * \retval      Other errors
 *
 */
quarklink_return_t quarklink_enrol(quarklink_context_t *ctx);

/**
 * \brief     	Request the current device status from QuarkLink (\c POST /device_status).
 *
 * On \c QUARKLINK_SUCCESS, use the \c quarklink_is_device_*() helpers below.
 * The server reports one lifecycle state (enrolled, not enrolled, revoked, or pending revoke).
 * Certificate expired and firmware update available may also be reported alongside that state.
 *
 * \param[in,out] ctx the QuarkLink context, make sure to initialise it with \ref quarklink_init() first
 * \retval QUARKLINK_SUCCESS Status received; use \c quarklink_is_device_*() helpers
 * \retval QUARKLINK_DEVICE_DOES_NOT_EXIST
 * \retval QUARKLINK_COMMUNICATION_ERROR Transport or TLS failure (e.g. unreachable instance)
 * \retval QUARKLINK_ERROR Unrecognised HTTP status, unrecognised status response, or other failure
 * \retval Other errors
 */
quarklink_return_t quarklink_status(const quarklink_context_t *ctx);

/**
 * \brief       Request a firmware update to QuarkLink, then runs the firmware over the air update.
 * \note        In case of a successful firmware update the device might restart
 *              before the function actually returns something.
 *              The key provided is used for validating the firmware and must match
 *              the key used to sign the firmware by QuarkLink.
 * \param[in,out] ctx the QuarkLink context, make sure to initialise it with \ref quarklink_init() first
 * \param       signing_key the public key used to validate the firmware, in PEM format. NULL for no key or 
 *              if the key was provisioned using QuarkLink provisioning task.
 * \return      The outcome of the update request
 * \retval      QUARKLINK_FW_UPDATE_UPDATED
 * \retval      QUARKLINK_FW_UPDATE_NO_UPDATE
 * \retval      QUARKLINK_FW_UPDATE_INVALID_SIGNATURE
 * \retval      QUARKLINK_FW_UPDATE_MISSING_SIGNATURE
 * \retval      QUARKLINK_FW_UPDATE_ERROR
 * \retval      QUARKLINK_COMMUNICATION_ERROR Transport, TLS, or firmware download read failure
 * \retval      QUARKLINK_ERROR Server response error, unrecognised HTTP status, or other failure
 * \retval      Other errors
 */
quarklink_return_t quarklink_firmware_update(const quarklink_context_t *ctx,
                                             const char *signing_key);

/**
 * \brief Persist QuarkLink instance connection settings to non-volatile storage.
 *
 * Stores the QuarkLink configuration details currently in the context, 
 * from the last \ref quarklink_init(). Call after init so the device can reconnect
 * without hard-coding these values. Does not store enrolment credentials — use 
 * \ref quarklink_persist_enrolment_context() after a successful \ref quarklink_enrol().
 *
 * Use \ref quarklink_load_stored_context() to restore both layers on startup.
 *
 * \param[in]   ctx the context to be stored. Needs to be initialised first.
 * \retval  QUARKLINK_SUCCESS
 * \retval  QUARKLINK_NOT_INITIALISED
 * \retval  QUARKLINK_NVM_ERROR
 */
quarklink_return_t quarklink_persist_context(const quarklink_context_t *ctx);

/**
 * \brief Persist enrolment credentials to non-volatile storage.
 *
 * Stores post-enrolment fields: device certificate, IoT Hub endpoint, port, root
 * certificate, scope ID, and firmware-update topic. Call after a successful
 * \ref quarklink_enrol() so the device can skip re-enrolment on restart.
 * Does not update the QuarkLink instance details persisted by
 * \ref quarklink_persist_context() — call both if you need a full cold-start restore.
 *
 * Use \ref quarklink_load_stored_context() to restore both layers on startup.
 *
 * \param[in]   ctx the context to be stored. Needs to be initialised first.
 * \retval  QUARKLINK_SUCCESS
 * \retval  QUARKLINK_NOT_INITIALISED
 * \retval  QUARKLINK_NVM_ERROR
 */
quarklink_return_t quarklink_persist_enrolment_context(const quarklink_context_t *ctx);

/**
 * \brief Load the QuarkLink context saved in non-volatile storage. 
 * Call this function to load the last configuration, to avoid re-enrolling the device after each restart.
 * Use `quarklink_persist_context()` to persist the context.
 * 
 * \param[out]  ctx the context to be updated with the stored configuration. Needs to be initialised first.
 * \retval  QUARKLINK_SUCCESS
 * \retval  QUARKLINK_NOT_INITIALISED
 * \retval  QUARKLINK_CONTEXT_NO_CREDENTIALS_STORED
 * \retval  QUARKLINK_CONTEXT_NO_ENROLMENT_INFO_STORED
 * \retval  QUARKLINK_CONTEXT_NOTHING_STORED
 * \retval  QUARKLINK_NVM_ERROR
 */
quarklink_return_t quarklink_load_stored_context(quarklink_context_t *ctx);

/**
 * \brief Only delete the enrolment details from non-volatile storage.
 * Enrolment details include: device_cert, IoT Hub endpoint, port and certificate, 
 * scope_id, firmware update topic.
 * QuarkLink instance details stored by \ref quarklink_persist_context() are left intact.
 * 
 * \param[in]  ctx the context. Needs to be initialised first.
 * \retval  QUARKLINK_SUCCESS
 * \retval  QUARKLINK_ERROR
 */
quarklink_return_t quarklink_delete_enrolment_context(const quarklink_context_t *ctx);

/**
 * \brief Delete QuarkLink instance details from non-volatile storage.
 * Enrolment credentials stored separately are not removed — use
 * \ref quarklink_delete_enrolment_context() for those.
 * 
 * \param[in]  ctx the context. Needs to be initialised first.
 * \retval  QUARKLINK_SUCCESS
 * \retval  QUARKLINK_ERROR
 */
quarklink_return_t quarklink_delete_context(const quarklink_context_t *ctx);

/**
 * \brief Check if the device is currently enrolled.
 *
 * Call after a successful \ref quarklink_status().
 *
 * \param[in] ctx QuarkLink context.
 * \note Lifecycle helpers (\ref quarklink_is_device_enrolled(), \ref quarklink_is_device_not_enrolled(),
 * \ref quarklink_is_device_revoked(), \ref quarklink_is_device_pending_revoke()) are mutually
 * exclusive: at most one is true per status response.
 *
 * \return 1 if true, 0 if false
 */
int quarklink_is_device_enrolled(const quarklink_context_t *ctx);

/**
 * \brief Check if the device is currently not enrolled.
 *
 * Call after a successful \ref quarklink_status(). See \ref quarklink_is_device_enrolled()
 * for lifecycle flag behaviour.
 *
 * \return 1 if true, 0 if false
 */
int quarklink_is_device_not_enrolled(const quarklink_context_t *ctx);

/**
 * \brief Check if the device is currently revoked.
 *
 * Call after a successful \ref quarklink_status(). See \ref quarklink_is_device_enrolled()
 * for lifecycle flag behaviour.
 *
 * \return 1 if true, 0 if false
 */
int quarklink_is_device_revoked(const quarklink_context_t *ctx);

/**
 * \brief Check if the device is pending revoke.
 *
 * Call after a successful \ref quarklink_status(). See \ref quarklink_is_device_enrolled()
 * for lifecycle flag behaviour.
 *
 * \return 1 if true, 0 if false
 */
int quarklink_is_device_pending_revoke(const quarklink_context_t *ctx);

/**
 * \brief Check if the device certificate is expired.
 *
 * Call after a successful \ref quarklink_status(). May be true together with a lifecycle
 * flag (for example enrolled and certificate expired).
 *
 * \return 1 if true, 0 if false
 */
int quarklink_is_device_certificate_expired(const quarklink_context_t *ctx);

/**
 * \brief Check if a firmware update is available for the device.
 *
 * Call after a successful \ref quarklink_status(). May be true together with a lifecycle
 * flag (for example enrolled and firmware update required).
 *
 * \return 1 if true, 0 if false
 */
int quarklink_is_device_fw_update_available(const quarklink_context_t *ctx);

/**
 * \brief Extract the QuarkLink instance hostname.
 *
 * Returns the instance hostname loaded in the internal context.
 * Works after \ref quarklink_init() and after \ref quarklink_load_stored_context().
 *
 * \param[in]  ctx        Initialised QuarkLink context.
 * \param[out] buffer     Output buffer (\ref QUARKLINK_MAX_ENDPOINT_LENGTH bytes).
 * \param[in]  buffer_len Size of \p buffer in bytes.
 *
 * \retval QUARKLINK_SUCCESS
 * \retval QUARKLINK_INVALID_PARAMETER
 * \retval QUARKLINK_NOT_INITIALISED
 */
quarklink_return_t quarklink_get_endpoint(const quarklink_context_t *ctx,
                                          char *buffer,
                                          size_t buffer_len);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif // _QUARKLINK_H
