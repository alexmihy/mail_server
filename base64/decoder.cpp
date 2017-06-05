#include "decoder.h"

#include <string.h>
#include <stdio.h>

const char Decoder::encoding_table[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3',
	'4', '5', '6', '7', '8', '9', '+', '/'
};

char* Decoder::decoding_table = (char*)malloc(256);

const int Decoder::mod_table[] = {0, 2, 1};

const char Decoder::base36[36] = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8',
	'9','A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 
	'I', 'J','K', 'L', 'M', 'N', 'O', 'P', 'Q', 
	'R', 'S', 'T','U', 'V', 'W', 'X', 'Y', 'Z'
};

Decoder::Decoder() 
{
	//decoding_table = (char*)malloc(256);

// 	for (int i = 0; i < 64; i++)
// 			decoding_table[(unsigned char) encoding_table[i]] = i;
}

Decoder::~Decoder() 
{
// 	free((void*)decoding_table);
}

void Decoder::Init() 
{
	for (int i = 0; i < 64; i++)
			decoding_table[(unsigned char) encoding_table[i]] = i;
}

char* Decoder::Base64Decode(
	const char *data, 
	size_t input_length, 
	size_t *output_length) 
{ 
	if (input_length % 4 != 0) return NULL;

	*output_length = input_length / 4 * 3;
	if (data[input_length - 1] == '=') (*output_length)--;
	if (data[input_length - 2] == '=') (*output_length)--;

	char *decoded_data = (char*)malloc(*output_length + 1);
	if (decoded_data == NULL) return NULL;

	for (size_t i = 0, j = 0; (data[i] != '\n' || data[i] != '\r') && i < input_length;) {
		uint32_t sextet_a = data[i] == '=' ? 0 & i++ : decoding_table[(size_t)data[i++]];
		uint32_t sextet_b = data[i] == '=' ? 0 & i++ : decoding_table[(size_t)data[i++]];
		uint32_t sextet_c = data[i] == '=' ? 0 & i++ : decoding_table[(size_t)data[i++]];
		uint32_t sextet_d = data[i] == '=' ? 0 & i++ : decoding_table[(size_t)data[i++]];

		uint32_t triple = (sextet_a << 3 * 6)
		+ (sextet_b << 2 * 6)
		+ (sextet_c << 1 * 6)
		+ (sextet_d << 0 * 6);

		if (j < *output_length) decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
		if (j < *output_length) decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
		if (j < *output_length) decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
	}
	decoded_data[*output_length] = '\0';
	
	return decoded_data;
}

char* Decoder::Base64Encode(
	const char *data, 
	size_t input_length,
	size_t *output_length) 
{ 
	*output_length = 4 * ((input_length + 2) / 3);

	char *encoded_data = (char*)malloc(*output_length);
	if (encoded_data == NULL) return NULL;

	for (size_t i = 0, j = 0; i < input_length;) {
		uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
		uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
		uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

		uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

		encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
		encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
		encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
		encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
	}

	
	for (int i = 0; i < mod_table[input_length % 3]; i++)
		encoded_data[*output_length - 1 - i] = '=';
	encoded_data[*output_length] = '\0';

	return encoded_data;
}

char* Decoder::Base36Encode(long unsigned int value) 
{ 
	/* log(2**64) / log(36) = 12.38 => max 13 char + '\0' */
	char buffer[14];
	unsigned int offset = sizeof(buffer);

	buffer[--offset] = '\0';
	do {
		buffer[--offset] = base36[value % 36];
	} while (value /= 36);

	return strdup(&buffer[offset]); // warning: this must be free-d by the user
}

long unsigned int Decoder::Base36Decode(const char *data) 
{ 
	return strtoul(data, NULL, 36);
}