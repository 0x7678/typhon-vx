#include <osmocom/bb/common/osmocom_data.h>

struct custom_ms {
	char dat_file[1024];
	uint32_t tmsi;
};


void init_custom_ms(struct osmocom_ms *ms);
void detroy_custom_ms(struct osmocom_ms *ms);
struct custom_ms *get_custom_ms(struct osmocom_ms *ms);
