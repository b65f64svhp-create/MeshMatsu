const char *mesh_radio_delivery_text(const mesh_message_t *m) {
    if (!m) return "No acknowledgement recorded";
    if (m->delivery == MESH_DELIVERY_ACKNOWLEDGED) return m->peer_node ? "Acknowledged by recipient" : "Delivered to mesh";
    if (m->delivery == MESH_DELIVERY_PENDING) return m->peer_node ? "Waiting for recipient ACK" : "Waiting for mesh confirmation";
    if (m->delivery != MESH_DELIVERY_FAILED) return "No acknowledgement recorded";
    if (m->failure == MESH_FAILURE_TIMEOUT) return "No ACK within 90 seconds";
    if (m->failure == MESH_FAILURE_RESTARTED) return "Unconfirmed: app restarted";
    if (m->failure != MESH_FAILURE_RADIO) return "Delivery not confirmed";
    // Matches Routing.Error in the radio's Meshtastic 2.7.18 protobufs.
    switch (m->error_reason) {
        case 1: return "Failed: no route";
        case 2: return "Failed: negative acknowledgement";
        case 3: return "Radio timed out waiting for ACK";
        case 4: return "Failed: radio interface unavailable";
        case 5: return "No ACK: radio retries exhausted";
        case 6: return "Failed: no matching channel";
        case 7: return "Failed: packet too large";
        case 8: return "Failed: no application response";
        case 9: return "Blocked: duty-cycle limit";
        case 32: return "Failed: invalid request";
        case 33: return "Failed: not authorized";
        case 34: return "Failed: public-key encryption";
        case 35: return "Failed: recipient public key unknown";
        case 36: return "Failed: admin session expired";
        case 37: return "Failed: admin key unauthorized";
        case 38: return "Blocked: rate limit";
        case 39: return "Failed: invalid recipient public key";
        default: {
            static char unknown[48];
            snprintf(unknown, sizeof(unknown), "Radio error %lu", (unsigned long)m->error_reason);
            return unknown;
        }
    }
}
