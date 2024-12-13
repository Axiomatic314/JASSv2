use std::{ffi::c_void, slice::{from_raw_parts, from_raw_parts_mut}};
use compress::{Compressor, SimdBPandStreamVbyte};

#[no_mangle]
pub extern fn ioqp_encode(encoded: *mut c_void, encoded_buffer_length: usize, source: *const u32, source_integers: usize) -> usize { 
    let mut bytes: usize = 0;
    let mut initial: u32 = 0;
    let compressed = unsafe { from_raw_parts_mut(encoded as *mut u8, encoded_buffer_length) };
    let docs: &[u32] = unsafe { from_raw_parts(source, source_integers) };
    docs.chunks(compress::BLOCK_LEN).for_each(|chunk|{
        let compressed_len = match chunk.len() {
            //full blocks -> SIMDBP
            compress::BLOCK_LEN => {
                SimdBPandStreamVbyte::compress_sorted_full(initial, chunk, &mut compressed[bytes..])
            }
            //non-full block -> streamvbyte
            _ => SimdBPandStreamVbyte::compress_sorted(initial, chunk, &mut compressed[bytes..]),
        };
        initial = *chunk.last().expect("chunk is non-empty");
        bytes += compressed_len;
    });

    return bytes;
}

#[no_mangle]
pub extern fn ioqp_decode(decoded: *mut u32, integers_to_decode: usize, source: *const c_void, source_length: usize) {
    let mut remaining_u32s = integers_to_decode;
    let mut bytes: usize = 0;
    let mut initial: u32 = 0;
    let compressed: &[u8] = unsafe { from_raw_parts(source as *mut u8, source_length) };
    let docs: &mut [u32] = unsafe { from_raw_parts_mut(decoded, integers_to_decode) };
    docs.chunks_mut(compress::BLOCK_LEN).for_each(|chunk| {
        match remaining_u32s {
            //non-full block -> streamvbyte
            1..=compress::BLOCK_LEN_M1 => {
               SimdBPandStreamVbyte::decompress_sorted(initial, &compressed[bytes..], chunk);
               remaining_u32s = 0;
            },
            //full blocks -> SIMDBP
            _ => {
                remaining_u32s -= compress::BLOCK_LEN;
                let compressed_len = SimdBPandStreamVbyte::decompress_sorted_full(initial, &compressed[bytes..], chunk);
                bytes += compressed_len;
                initial = unsafe { *chunk.get_unchecked(compress::BLOCK_LEN - 1) };
            }
           
        }
    });
}



// The following was ripped from IOQP/src/compress.rs
pub mod compress {
    use bitpacking::BitPacker;
    use byteorder::WriteBytesExt;

    type SimdbpCompressor = bitpacking::BitPacker4x;

    pub const BLOCK_LEN: usize = SimdbpCompressor::BLOCK_LEN;
    pub const BLOCK_LEN_M1: usize = BLOCK_LEN - 1;


    pub trait Compressor {
        fn compress_sorted_full(initial: u32, input: &[u32], output: &mut [u8]) -> usize;
        fn compress_sorted(initial: u32, input: &[u32], output: &mut [u8]) -> usize;
        fn decompress_sorted_full(initial: u32, input: &[u8], output: &mut [u32]) -> usize;
        fn decompress_sorted(initial: u32, input: &[u8], output: &mut [u32]) -> usize;
    }

    #[derive(Debug)]
    pub struct SimdBPandStreamVbyte;

    impl Compressor for SimdBPandStreamVbyte {
        fn compress_sorted_full(initial: u32, input: &[u32], mut output: &mut [u8]) -> usize {
            let bitpacker = SimdbpCompressor::new();
            let num_block_bits = bitpacker.num_bits_sorted(initial, input);
            output.write_u8(num_block_bits).unwrap();
            let bytes = bitpacker.compress_sorted(initial, input, &mut *output, num_block_bits);
            bytes + 1
        }
        fn compress_sorted(initial: u32, input: &[u32], output: &mut [u8]) -> usize {
            streamvbyte::encode_delta_to_buf(input, &mut *output, initial).unwrap()
        }
        fn decompress_sorted_full(initial: u32, input: &[u8], output: &mut [u32]) -> usize {
            let bitpacker = SimdbpCompressor::new();
            let num_bits = unsafe { *input.get_unchecked(0) };
            let compressed_len = (num_bits as usize * BLOCK_LEN) >> 3;
            let compressed = unsafe { input.get_unchecked(1..=compressed_len) };
            let bytes = bitpacker.decompress_sorted(initial, compressed, output, num_bits);
            bytes + 1
        }
        fn decompress_sorted(initial: u32, input: &[u8], output: &mut [u32]) -> usize {
            streamvbyte::decode_delta(input, output, initial)
        }
    }

}
