#pragma once
#include "packet_runner.h"
#include "esp_err.h"
typedef struct {bool valid;uint32_t moves,drops;} packet_record_t;
typedef struct {packet_record_t route[PACKET_LEVELS],run;uint8_t highest_route;} packet_scores_t;
// Fewer drops wins; moves break ties. Updates only on valid completed deliveries.
bool packet_scores_record(packet_scores_t *scores,const packet_runner_t *game);
bool packet_scores_merge(packet_scores_t *scores,const packet_scores_t *other);
esp_err_t packet_scores_load(packet_scores_t *scores);
esp_err_t packet_scores_save(const packet_scores_t *scores);
