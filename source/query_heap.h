/*
	QUERY_HEAP.H
	------------
	Copyright (c) 2021 Andrew Trotman
	Released under the 2-clause BSD license (See:https://en.wikipedia.org/wiki/BSD_licenses)
*/
/*!
	@file
	@brief Everything necessary to process a query using a heap to store the top-k.
	@author Andrew Trotman
	@copyright 2021 Andrew Trotman
*/
#pragma once

#include "heap.h"
#include "query.h"
#include "pointer_box.h"
#include "top_k_qsort.h"
#include "accumulator_2d.h"
#include "exception_done.h"
#include "compress_integer_variable_byte.h"

namespace JASS
	{
	/*
		CLASS QUERY_HEAP
		----------------
	*/
	/*!
		@brief Everything necessary to process a query (using a heap) is encapsulated in an object of this type
	*/
	template <typename ACCUMULATOR_ARRAY>
	class query_heap : public query
		{
		private:
			typedef pointer_box<ACCUMULATOR_TYPE> accumulator_pointer;

		private:
			ACCUMULATOR_ARRAY accumulators;											///< The accumulators, one per document in the collection
			DOCID_TYPE needed_for_top_k;												///< The number of results we still need in order to fill the top-k
			ACCUMULATOR_TYPE zero;														///< Constant zero used for pointer dereferenced comparisons
			accumulator_pointer accumulator_pointers[MAX_TOP_K];				///< Array of pointers to the top k accumulators
			heap<accumulator_pointer> top_results;									///< Heap containing the top-k results
			bool sorted;																	///< has heap and accumulator_pointers been sorted (false after rewind() true after sort())
			ACCUMULATOR_TYPE top_k_lower_bound;										///< Lowest possible score to enter the top k
			docid_rsv_pair next_result;												///< A single result, used but get_first() and get_next()
			DOCID_TYPE next_result_location;											///< Used by get_first() and get_next() to determine which result is next

		public:
			/*
				QUERY_HEAP::QUERY_HEAP()
				------------------------
			*/
			/*!
				@brief Constructor
			*/
			query_heap(compress_integer &codex) :
				query(codex),
				zero(0),
				top_results(accumulator_pointers, top_k)
				{
				rewind();
				}

			/*
				QUERY_HEAP::~QUERY_HEAP()
				-------------------------
			*/
			/*!
				@brief Destructor
			*/
			virtual ~query_heap()
				{
				/* Nothing */
				}

			/*
				QUERY_HEAP::INIT()
				------------------
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
				QUERY_HEAP::GET_FIRST()
				-----------------------
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
				QUERY_HEAP::GET_NEXT()
				----------------------
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
				next_result.rsv = accumulators.get_value(id);

				next_result_location++;

				return &next_result;
				}

			/*
				QUERY_HEAP::REWIND()
				--------------------
			*/
			/*!
				@brief Clear this object after use and ready for re-use
			*/
			virtual void rewind(ACCUMULATOR_TYPE smallest_possible_rsv = 0, ACCUMULATOR_TYPE top_k_lower_bound = 1, ACCUMULATOR_TYPE largest_possible_rsv = 0)
				{
				sorted = false;
				zero = 0;
				accumulator_pointers[0] = &zero;
				accumulators.rewind();
				needed_for_top_k = this->top_k;
				this->top_k_lower_bound = top_k_lower_bound;
				query::rewind(largest_possible_rsv);
				}

			/*
				QUERY_HEAP::SORT()
				------------------
			*/
			/*!
				@brief sort this resuls list before iteration over it.
			*/
			virtual void sort(void)
				{
				if (!sorted)
					{
//					std::partial_sort(accumulator_pointers + needed_for_top_k, accumulator_pointers + top_k, accumulator_pointers + top_k);
					top_k_qsort::sort(accumulator_pointers + needed_for_top_k, top_k - needed_for_top_k, top_k);
					sorted = true;
					}
				}

			/*
				QUERY_HEAP::ADD_RSV()
				---------------------
			*/
			/*!
				@brief Add weight to the rsv for document document_id
				@param document_id [in] which document to increment
				@param score [in] the amount of weight to add
			*/
			forceinline void add_rsv(DOCID_TYPE document_id, ACCUMULATOR_TYPE score)
				{
				accumulator_pointer which = &accumulators[document_id];			/* This will create the accumulator if it doesn't already exist. */
				*which.pointer() += score;
				/*
					accumulator is less than the heap entry value
				*/
				if (*which.pointer() < top_k_lower_bound)
					return;
				/*
					the heap isn't full yet - so change only happens if we're a new addition (i.e. the old value was a 0)
				*/
				if (needed_for_top_k > 0)
					{
					/*
						we weren't already in the heap
					*/
					if (*which.pointer() - score < top_k_lower_bound)
						{
						accumulator_pointers[--needed_for_top_k] = which;
						if (needed_for_top_k == 0)
							{
							top_results.make_heap();
							if (top_k_lower_bound != 1)
								throw Done(); /* We must be using the Oracle, and we must have filled the top-k and so we can stop processing this query. */
							top_k_lower_bound = *accumulator_pointers[0]; /* set the new bottom of heap value */
							}
						}
					return;
					}
				/*
					accumulator is equal to the heap entry value, now we need to tie break
				*/
				if (*which.pointer() == top_k_lower_bound)
					{
					if (which.pointer() < accumulator_pointers[0].pointer())
						return;
					top_results.push_back(which); /* we're not in the heap so add this accumulator to the heap */
					top_k_lower_bound = *accumulator_pointers[0];
					return;
					}
				/*
					accumulator exceeds the heap entry value, we now need to figure out if the accumulator was already in the heap
					if the old value didn't exceed the heap entry, or it was equal and we failed the tie-break then we weren't in it
					otherwise we were and need to be promoted
				*/
				if (*which.pointer() - score < top_k_lower_bound || (*which.pointer() - score == top_k_lower_bound && which.pointer() < accumulator_pointers[0].pointer()))
					top_results.push_back(which);
				else
					{
					auto at = top_results.find(which); /* we're already in there so find us and reshuffle the heap. */
					top_results.promote(which, at); /* we're already in the heap so promote this document */
					}
				top_k_lower_bound = *accumulator_pointers[0]; /* set the new bottom of heap value */
				}

			/*
				QUERY_HEAP::DECODE_WITH_WRITER()
				--------------------------------
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
					try
						{
						add_rsv(*current, impact);
						}
					catch (Done&)
						{
						break;
						}
				}

			/*
				QUERY_HEAP::UNITTEST()
				----------------------
			*/
			/*!
				@brief Unit test this class
			*/
			static void unittest(void)
				{
				std::vector<std::string> keys = {"one", "two", "three", "four"};
				compress_integer_variable_byte codex;
				query_heap *query_object = new query_heap(codex);
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

				puts("query_heap::PASSED");
				}
		};
	}
