#ifndef DECODER_H_SENTRY
#define DECODER_H_SENTRY

#include <stdint.h>
#include <stdlib.h>

class Decoder 
{
	static const char encoding_table[];
	static char *decoding_table;
	static const int mod_table[];
	static const char base36[];
	
public:
	Decoder();
	~Decoder();
	
	static void Init();
	
	//memory should be free'd by user
	static char* Base64Decode(
		const char *data,
		size_t input_length,
		size_t *output_length
	);

	//memory should be free'd by user
	static char* Base64Encode(
		const char *data,
		size_t input_length,
		size_t *output_length
	);

	static char* Base36Encode(long unsigned int value);
	static long unsigned int Base36Decode(const char *data);
	
};

#endif