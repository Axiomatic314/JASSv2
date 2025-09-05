/*
	QUERY_BLOCK_MAX.H
	-----------------
	Copyright (c) 2025 Andrew Trotman
	Released under the 2-clause BSD license (See:https://en.wikipedia.org/wiki/BSD_licenses)
*/
/*!
	@file
	@brief Everything necessary to process a query using the block-max approach of IOQP
	@details see J. Mackenzie, M. Petri, L. Gallagher (2022), IOQP: A simple Impact-Ordered Query Processor written in Rust, Proceedings of DESIRES 2022, pp 22-34.
	(https://github.com/jmmackenzie/ioqp)
	@author Andrew Trotman
	@copyright 2025 Andrew Trotman
*/
#pragma once

#include "maths.h"
#include "query.h"
#include "compress_integer.h"
#include "accumulator_block_max.h"

namespace JASS
	{
	/*
		CLASS QUERY_BLOCK_MAX
		---------------------
	*/
	/*!
		@brief Everything necessary to process a query is encapsulated in an object of this type
	*/
	class query_block_max : public query
		{
		private:
			typedef pointer_box<ACCUMULATOR_TYPE> accumulator_pointer;

		private:
			accumulator_block_max<ACCUMULATOR_TYPE, MAX_DOCUMENTS> accumulators;			///< The accumulators, one per document in the collection
			bool sorted;																	///< Has the top-k been generates (false after rewind() true after sort())
			docid_rsv_pair next_result;												///< A single result, used but get_first() and get_next()
			DOCID_TYPE next_result_location;											///< Used by get_first() and get_next() to determine which result is next
			accumulator_pointer accumulator_pointers[MAX_TOP_K];				///< Array of pointers to the top k accumulators
			heap<accumulator_pointer> top_results;									///< Heap containing the top-k results
			DOCID_TYPE needed_for_top_k;												///< The number of results we still need in order to fill the top-k

		public:
			/*
				QUERY_BLOCK_MAX::QUERY_BLOCK_MAX()
				----------------------------------
			*/
			/*!
				@brief Constructor
			*/
			query_block_max(compress_integer &codex) :
				query(codex),
				top_results(accumulator_pointers, top_k)
				{
				rewind();
				}

			/*
				QUERY_BLOCK_MAX::~QUERY_BLOCK_MAX()
				-----------------------------------
			*/
			/*!
				@brief Destructor
			*/
			virtual ~query_block_max()
				{
				/* Nothing */
				}

			/*
				QUERY_BLOCK_MAX::INIT()
				-----------------------
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
				query::init(primary_keys, documents, top_k);
				accumulators.init(documents, width);
				top_results.set_top_k(top_k);
				}

			/*
				QUERY_BLOCK_MAX::GET_FIRST()
				----------------------------
			*/
			/*!
				@brief Retrun the top result.
				@return The first (i.e. top) result in the results list.
			*/
			virtual docid_rsv_pair *get_first(void)
				{
				sort();
				next_result_location = 0;
				return get_next();
				}

			/*
				QUERY_BLOCK_MAX::GET_NEXT()
				---------------------------
			*/
			/*!
				@brief After calling get_first(), return the next result
				@return The next result in the results list, or NULL if at end of list
			*/
			virtual docid_rsv_pair *get_next(void)
				{
				if (next_result_location >= top_k - needed_for_top_k)
					return NULL;

				size_t id = accumulators.get_index(accumulator_pointers[top_k - next_result_location - 1].pointer());
				next_result.document_id = id;
				next_result.primary_key = &((*primary_keys)[id]);
				next_result.rsv = accumulators[id];

				next_result_location++;

				return &next_result;
				}

			/*
				QUERY_BLOCK_MAX::REWIND()
				-------------------------
			*/
			/*!
				@brief Clear this object after use and ready for re-use
			*/
			virtual void rewind(ACCUMULATOR_TYPE smallest_possible_rsv = 0, ACCUMULATOR_TYPE top_k_lower_bound = 1, ACCUMULATOR_TYPE largest_possible_rsv = 0)
				{
				sorted = false;
				accumulators.rewind();
				needed_for_top_k = this->top_k;
				query::rewind(largest_possible_rsv);
				}

			/*
				QUERY_BLOCK_MAX::SORT()
				-----------------------
			*/
			/*!
				@brief sort this resuls list before iteration over it.
				@details  In block_max processing, we scan the block_max scores looking for instances
				where the score is greater than the bottom of the heap. If we find one then its necessary
				to scan that block looking for instances where an accumulator is greater than the bottom
				of the heap.  The heap is then built from those accumulators bu pushing pointers to the
				accumulators onto the heap.  Then we sort the heap.
			*/
			virtual void sort(void)
				{
				if (!sorted)
					{
					/*
						Here we scan the block looking for values that need to be inserted into the top-k heap.  If a
						block max score is less than the bottom of the heap we can skip the block, thus avoiding a full scan
					*/
					ACCUMULATOR_TYPE bottom_of_heap = 0;
					ACCUMULATOR_TYPE *which_accumulator = accumulators.accumulator;
					ACCUMULATOR_TYPE *which_block = accumulators.block_max;
					ACCUMULATOR_TYPE *end = accumulators.block_max + accumulators.number_of_blocks;
					while (which_block < end)
						{
						if (*which_block > bottom_of_heap)
							{
							/*
								There's a score in this block that's larger than the bottom of the heap, so a potential candidate
							*/
							ACCUMULATOR_TYPE *current_accumulator = which_accumulator;
							ACCUMULATOR_TYPE *end_accumulator = which_accumulator + accumulators.width;
							while (current_accumulator < end_accumulator)
								{
								if (*current_accumulator > bottom_of_heap)
									{
									/*
										We have an accumulator ready for the heap
									*/
									if (needed_for_top_k > 0)
										{
										/*
											Heap isn't full, so just top it up
										*/
										accumulator_pointers[--needed_for_top_k] = current_accumulator;
										if (needed_for_top_k == 0)
											{
											/*
												The heap array is now full so build the heap
											*/
											top_results.make_heap();
											bottom_of_heap = *accumulator_pointers[0]; /* set the new bottom of heap value */
											}
										}
									else
										{
										/*
											The heap is already full so evict te bottom one and add a new one
										*/
										top_results.push_back(current_accumulator);
										bottom_of_heap = *accumulator_pointers[0]; /* set the new bottom of heap value */
										}
									}
								current_accumulator++;
								}
							}
						which_accumulator += accumulators.width;
						which_block++;
						}

					/*
						Now sort the heap array to get the answers in rank order.
					*/
					top_k_qsort::sort(accumulator_pointers + needed_for_top_k, top_k - needed_for_top_k, top_k);
					sorted = true;
					}
				}

			/*
				QUERY_BLOCK_MAX::ADD_RSV()
				--------------------------
			*/
			/*!
				@brief Add weight to the rsv for document document_id
				@param document_id [in] which document to increment
				@param score [in] the amount of weight to add
			*/
			forceinline void add_rsv(DOCID_TYPE document_id, ACCUMULATOR_TYPE score)
				{
				accumulators.add(document_id, score);
				}

			/*
				QUERY_BLOCK_MAX::DECODE_WITH_WRITER()
				-------------------------------------
			*/
			/*!
				@brief Given the integer decoder, the number of integes to decode, and the compressed sequence, decompress (but do not process).
				@param integers [in] The number of integers that are compressed.
				@param compressed [in] The compressed sequence.
				@param compressed_size [in] The length of the compressed sequence.
			*/
			virtual void decode_with_writer(size_t integers, const void *compressed, size_t compressed_size)
				{
				DOCID_TYPE *buffer = reinterpret_cast<DOCID_TYPE *>(decompress_buffer.data());
				codex.decode(buffer, integers, compressed, compressed_size);

				/*
					D1-decode inplace with SIMD instructions then process one at a time
				*/
				simd::cumulative_sum_256(buffer, integers);

				/*
					Process the d1-decoded postings list.  We ask the compiler to unroll the loop as it
					appears to be as fast as manually unrolling it.
				*/
				const DOCID_TYPE *end = buffer + integers;
#if defined(__clang__)
				#pragma unroll 8
#elif defined(__GNUC__) || defined(__GNUG__)
				#pragma GCC unroll 8
#endif
				for (DOCID_TYPE *current = buffer; current < end; current++)
					add_rsv(*current, impact);
				}

			/*
				QUERY_BLOCK_MAX::UNITTEST()
				---------------------------
			*/
			/*!
				@brief Unit test this class
			*/
			static void unittest(void)
				{
				std::vector<std::string> keys = {"one", "two", "three", "four"};
				compress_integer_variable_byte codex;
				query_block_max *query_object = new query_block_max(codex);
				query_object->init(keys, 1024, 2);
				std::ostringstream string;

				/*
					Check the rsv stuff
				*/
				query_object->add_rsv(2, 10);
				query_object->add_rsv(3, 20);
				query_object->add_rsv(2, 2);
				query_object->add_rsv(1, 1);
				query_object->add_rsv(1, 14);

				for (docid_rsv_pair *rsv = query_object->get_first(); rsv != NULL; rsv = query_object->get_next())
					string << "<" << rsv->document_id << "," << (uint32_t)rsv->rsv << ">";
				JASS_assert(string.str() == "<3,20><1,15>");

				/*
					Check the parser
				*/
				size_t times = 0;
				query_object->parse(std::string("one two three"));
				for (const auto &term : query_object->terms())
					{
					times++;
					if (times == 1)
						JASS_assert(term.token() == "one");
					else if (times == 2)
						JASS_assert(term.token() == "two");
					else if (times == 3)
						JASS_assert(term.token() == "three");
					}

				puts("query_block_max::PASSED");
				}
		};
	}
