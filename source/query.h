/*
	QUERY.H
	-------
	Copyright (c) 2017 Andrew Trotman
	Released under the 2-clause BSD license (See:https://en.wikipedia.org/wiki/BSD_licenses)
*/
/*!
	@file
	@brief Everything necessary to process a query.  Subclassed in order to get add_rsv()
	@author Andrew Trotman
	@copyright 2017 Andrew Trotman
*/
#pragma once

#include <limits>

#include <immintrin.h>

#include "forceinline.h"
#include "parser_query.h"
#include "query_term_list.h"
#include "allocator_memory.h"
#include "compress_integer.h"

namespace JASS
	{
	/*
		CLASS QUERY
		-----------
	*/
	/*!
		@brief Everything necessary to process a query is encapsulated in an object of this type
	*/
	class query
		{
		public:
//			typedef uint32_t ACCUMULATOR_TYPE;									///< the type of an accumulator (probably a uint16_t)
//			typedef uint16_t ACCUMULATOR_TYPE;									///< the type of an accumulator (probably a uint16_t)
			typedef uint8_t ACCUMULATOR_TYPE;									///< the type of an accumulator (probably a uint16_t)
			typedef uint32_t DOCID_TYPE;										///< the type of a document id (from a compressor)

		public:
			static constexpr size_t MAX_DOCUMENTS = 200'000'000;					///< the maximum number of documents an index can hold
			static constexpr size_t MAX_TOP_K = 1'000;							///< the maximum top-k value
			static constexpr size_t MAX_RSV = (std::numeric_limits<ACCUMULATOR_TYPE>::max)();

		public:
			/*
				CLASS QUERY::PRINTER
				--------------------
			*/
			class printer
				{
				public:
					/*!
						@brief print (somehow) the ordered pair of document_id and score.  This might involve processing it somehow too
						@param document_id [in] The document identifier
						@param score [in] The impact score of this term in this document.
					*/
					virtual void add_rsv(DOCID_TYPE document_id, ACCUMULATOR_TYPE score) = 0;
				};

			/*
				CLASS QUERY::DOCID_RSV_PAIR
				---------------------------
			*/
			/*!
				@brief Literally a <document_id, rsv> ordered pair.
			*/
			class docid_rsv_pair
				{
				public:
					size_t document_id;							///< The document identifier
					const std::string *primary_key;			///< The external identifier of the document (the primary key)
					ACCUMULATOR_TYPE rsv;						///< The rsv (Retrieval Status Value) relevance score
				};

		protected:
			ACCUMULATOR_TYPE impact;													///< The impact score to be added on a call to add_rsv()

			allocator_pool memory;														///< All memory allocation happens in this "arena"
			std::vector<DOCID_TYPE> decompress_buffer;							///< The delta-encoded decopressed integer sequence.
			DOCID_TYPE documents;														///< The numnber of documents this index contains

			parser_query parser;															///< Parser responsible for converting text into a parsed query
			query_term_list *parsed_query;											///< The parsed query
			const std::vector<std::string> *primary_keys;						///< A vector of strings, each the primary key for the document with an id equal to the vector index
			compress_integer &codex;													///< The decompressor to use.

		public:
			DOCID_TYPE top_k;																	///< The number of results to track.

		public:
			/*
				QUERY::QUERY()
				--------------
			*/
			/*!
				@brief Constructor
			*/
			query(compress_integer &codex) :
				impact(1),
				documents(0),
				parser(memory),
				parsed_query(nullptr),
				primary_keys(nullptr),
				codex(codex),
				top_k(0)
				{
				/*	 Nothing */
				}

			/*
				QUERY::INIT()
				-------------
			*/
			/*!
				@brief Initialise the object. MUST be called before first use.
				@param primary_keys [in] Vector of the document primary keys used to convert from internal document ids to external primary keys.
				@param documents [in] The number of documents in the collection.
				@param top_k [in]	The top-k documents to return from the query once executed.
				@param width [in] The width of the 2-d accumulators (if they are being used).
			*/
			virtual void init(const std::vector<std::string> &primary_keys, DOCID_TYPE documents = 1024, DOCID_TYPE top_k = 10, size_t width = 7)
				{
				this->primary_keys = &primary_keys;
				this->top_k = top_k;
				this->documents = documents;
				decompress_buffer.resize(64 + (documents * sizeof(DOCID_TYPE) + sizeof(decompress_buffer[0]) - 1) / sizeof(decompress_buffer[0]));			// we add 64 so that decompressors can overflow
				rewind(1, 1, 1);
				}

			/*
				QUERY::~QUERY()
				---------------
			*/
			/*!
				@brief Destructor
			*/
			virtual ~query()
				{
				delete parsed_query;
				}

			/*
				QUERY::PARSE()
				--------------
			*/
			/*!
				@brief Take the given query and parse it.
				@tparam STRING_TYPE Either a std::string or JASS::string.
				@param query [in] The query to parse.
			*/
			template <typename STRING_TYPE>
			void parse(const STRING_TYPE &query, parser_query::parser_type which_parser = parser_query::parser_type::query)
				{
				parser.parse(*parsed_query, query, which_parser);
				}

			/*
				QUERY::TERMS()
				--------------
			*/
			/*!
				@brief Return a reference to the parsed query.
				@return A reference to the parsed query.
			*/
			query_term_list &terms(void)
				{
				return *parsed_query;
				}

			/*
				QUERY::GET_FIRST()
				------------------
			*/
			/*!
				@brief Retrun the top result.
				@return The first (i.e. top) result in the results list.
			*/
			virtual docid_rsv_pair *get_first(void) = 0;

			/*
				QUERY::GET_NEXT()
				-----------------
			*/
			/*!
				@brief After calling get_first(), return the next result
				@return The next result in the results list, or NULL if at end of list
			*/
			virtual docid_rsv_pair *get_next(void) = 0;

			/*
				QUERY::REWIND()
				---------------
			*/
			/*!
				@brief Clear this object after use and ready for re-use
				@param smallest_possible_rsv [in] No rsv can be smaller than this (other than documents that are not found
				@param top_k_lower_bound [in] No rsv smaller than this can enter the top-k results list
				@param largest_possible_rsv [in] No rsv can be larger than this (but need no one need be this large)
			*/
			virtual void rewind(ACCUMULATOR_TYPE smallest_possible_rsv = 0, ACCUMULATOR_TYPE top_k_lower_bound = 0, ACCUMULATOR_TYPE largest_possible_rsv = 0)
				{
				delete parsed_query;
				parsed_query = new query_term_list;
				impact = 0;
				}

			/*
				QUERY::SET_IMPACT()
				-------------------
			*/
			/*!
				@brief Set the impact score to use in a push_back().
				@param score [in] The impact score to be added to accumulators.
			*/
			forceinline void set_impact(ACCUMULATOR_TYPE score)
				{
				impact = score;
				}

			/*
				QUERY::SORT()
				-------------
			*/
			/*!
				@brief sort this resuls list before iteration over it.
			*/
			virtual void sort(void) = 0;

			/*
				QUERY::DECODE_AND_PROCESS()
				---------------------------
			*/
			/*!
				@brief Given the integer decoder, the number of integes to decode, and the compressed sequence, decompress (but do not process).
				@param impact [in] The impact score to add for each document id in the list.
				@param integers [in] The number of integers that are compressed.
				@param compressed [in] The compressed sequence.
				@param compressed_size [in] The length of the compressed sequence.
			*/
			forceinline bool decode_and_process(ACCUMULATOR_TYPE impact, size_t integers, const void *compressed, size_t compressed_size)
				{
				set_impact(impact);
				return decode_with_writer(integers, compressed, compressed_size);
				}

			/*
				QUERY::DECODE_WITH_WRITER()
				---------------------------
			*/
			/*!
				@brief Given the integer decoder, the number of integes to decode, and the compressed sequence, decompress (but do not process).
				@param integers [in] The number of integers that are compressed.
				@param compressed [in] The compressed sequence.
				@param compressed_size [in] The length of the compressed sequence.
			*/
			virtual bool decode_with_writer(size_t integers, const void *compressed, size_t compressed_size) = 0;

			/*
				QUERY::DECODE_WITH_WRITER()
				---------------------------
			*/
			/*!
				@brief Given the integer decoder, the number of integes to decode, and the compressed sequence, decompress (but do not process).
				@param writer [in] the device to write to
				@param integers [in] The number of integers that are compressed.
				@param compressed [in] The compressed sequence.
				@param compressed_size [in] The length of the compressed sequence.
			*/
			virtual void decode_with_writer(query::printer &writer, size_t integers, const void *compressed, size_t compressed_size)
				{
				DOCID_TYPE *buffer = reinterpret_cast<DOCID_TYPE *>(decompress_buffer.data());
				codex.decode(buffer, integers, compressed, compressed_size);

				DOCID_TYPE id = 0;
				DOCID_TYPE *end = buffer + integers;
				for (auto *current = buffer; current < end; current++)
					{
					id += *current;
					writer.add_rsv(id, impact);
					}
				}

		};
	}
