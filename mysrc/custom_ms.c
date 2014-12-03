/*

DIRTY HACK!!!

Missuse some unused entry in osmocom_ms to store pointer to our structue

*/


#include <osmocom/bb/common/osmocom_data.h>

#include "custom_ms.h"



void init_custom_ms(struct osmocom_ms *ms) {

	struct custom_ms* ptr = malloc(sizeof(struct custom_ms));
	memset(ptr, '\0', sizeof(struct custom_ms));
	memcpy(ms->settings.emergency_imsi, &ptr, sizeof(ptr));
}

void destroy_custom_ms(struct osmocom_ms *ms) {

	struct custom_ms* ptr;
	memcpy(&ptr, ms->settings.emergency_imsi, sizeof(ptr));
	free(ptr);
}

struct custom_ms *get_custom_ms(struct osmocom_ms *ms) {

	struct custom_ms* ptr;
	memcpy(&ptr, ms->settings.emergency_imsi, sizeof(ptr));
	return ptr;
}
