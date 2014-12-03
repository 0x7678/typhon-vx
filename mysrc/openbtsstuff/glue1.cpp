#include "RxBurst.h"
#include "GSMCommon.h"
#include "GSML1FEC.h"



static GSM::TCHFACCHL1Decoder *d_tch_decoder[2];

extern "C" {
void tch_init() {

	d_tch_decoder[0] = new GSM::TCHFACCHL1Decoder(GSM::gFACCH_TCHFMapping);
	d_tch_decoder[0]->setMode(GSM::MODE_SPEECH_EFR);
//	d_tch_decoder[0]->setMode(GSM::MODE_SPEECH_FR);
	d_tch_decoder[1] = new GSM::TCHFACCHL1Decoder(GSM::gFACCH_TCHFMapping);
	d_tch_decoder[1]->setMode(GSM::MODE_SPEECH_EFR);
//	d_tch_decoder[1]->setMode(GSM::MODE_SPEECH_FR);
}

int tch_process_burst(char *burst, int ts, int fn, int ul) {

	int i;


	float decrypted_data_float[148];

	for (i = 0; i< 3; i++)
		decrypted_data_float[i] = 2;
	for (i = 3; i< 145; i++)
		decrypted_data_float[i] = burst[i-3];
	for (i = 145; i< 148; i++)
		decrypted_data_float[i] = 2;

	GSM::Time time(fn, ts);
	GSM::RxBurst rxbrst(decrypted_data_float, time);

	if ( d_tch_decoder[ul]->processBurst( rxbrst ) == true)
		return 1;
	else
		return 0;
}

unsigned char *get_voice_frame(int ul) {

          return d_tch_decoder[ul]->get_voice_frame();
}

int get_voice_frame_length(int ul) {

	return d_tch_decoder[ul]->get_voice_frame_length();
}


}
