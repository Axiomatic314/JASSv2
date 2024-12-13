#include "compress_integer_ioqp.h"

namespace JASS{

    /*
		COMPRESS_INTEGER_IOQP::ENCODE()
		---------------------------------------
	*/
    size_t compress_integer_ioqp::encode(void *encoded, size_t encoded_buffer_length, const integer *source, size_t source_integers){
        return ioqp_encode(encoded, encoded_buffer_length, source, source_integers);
    }

    /*
	    COMPRESS_INTEGER_IOQP::DECODE()
	    ---------------------------------------
	*/
    void compress_integer_ioqp::decode(integer *decoded, size_t integers_to_decode, const void *source, size_t source_length){
        ioqp_decode(decoded, integers_to_decode, source, source_length);
    }
}


