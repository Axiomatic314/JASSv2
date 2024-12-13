#pragma once

#include <stdint.h>
#include <stdlib.h>
#include "compress_integer.h"

extern "C" {

size_t ioqp_encode(void *encoded, size_t encoded_buffer_length, const uint32_t *source, size_t source_length);

void ioqp_decode(uint32_t *decoded, size_t integers_to_decode, const void *source, size_t source_length);

}

namespace JASS{
    /*
		CLASS COMPRESS_INTEGER_BITPACK_128
		-----------------------------------
	*/
	/*!
		@brief SIMD BP-128 as implemented in IOQP.
	*/
    class compress_integer_ioqp : public compress_integer {
        public:
            /*
				COMPRESS_INTEGER_IOQP::ENCODE()
				---------------------------------------
			*/
			/*!
				@brief Encode a sequence of integers returning the number of bytes used for the encoding, or 0 if the encoded sequence doesn't fit in the buffer.
				@param encoded [out] The sequence of bytes that is the encoded sequence.
				@param encoded_buffer_length [in] The length (in bytes) of the output buffer, encoded.
				@param source [in] The sequence of integers to encode.
				@param source_integers [in] The length (in integers) of the source buffer.
				@return The number of bytes used to encode the integer sequence, or 0 on error (i.e. overflow).
			*/
            virtual size_t encode(void *encoded, size_t encoded_buffer_length, const integer *source, size_t source_integers);

            /*
				COMPRESS_INTEGER_IOQP::DECODE()
				---------------------------------------
			*/
			/*!
				@brief Decode a sequence of integers encoded with this codex.
				@param decoded [out] The sequence of decoded integers.
				@param integers_to_decode [in] The minimum number of integers to decode (it may decode more).
				@param source [in] The encoded integers.
				@param source_length [in] The length (in bytes) of the source buffer.
			*/
			virtual void decode(integer *decoded, size_t integers_to_decode, const void *source, size_t source_length);

    };
}