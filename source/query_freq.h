/*
	QUERY_FREQ.H
	--------------
*/
#pragma once

#include "document.h"
#include "pointer_box.h"
#include "query.h"
#include "heap.h"
#include "simd.h"
#include "compress_integer_variable_byte.h"
#include "top_k_qsort.h"

namespace JASS
    {
    class query_freq : public query
    	{
     	private:
      		typedef pointer_box<ACCUMULATOR_TYPE> accumulator_pointer;
     	private:
      		ACCUMULATOR_TYPE accumulator[MAX_DOCUMENTS];							///< The accumulators, one per document in the collection
        	DOCID_TYPE needed_for_top_k;												///< The number of results we still need in order to fill the top-k
        	// accumulator_pointer accumulator_pointers[MAX_TOP_K];							///< Array of pointers to the top-k accumulators
         accumulator_pointer accumulator_pointers[MAX_DOCUMENTS];
         	heap<accumulator_pointer> top_results;							///< Heap containing the top-k results
         	uint32_t score_freq[MAX_RSV];							///< Array of document score frequencies
          	docid_rsv_pair next_result;							///< A single result, used by get_first() and get_next()
           	DOCID_TYPE next_result_location;							///< Used by get_first() and get_next() to determine which result is next
            bool sorted;																	///< Has accumulator_pointer been sorted (false after rewind() true after sort())

      	public:
     		/*
				QUERY_FREQ::QUERY_FREQ()
				----------------------------
			*/
			/*!
				@brief Constructor
			*/
       		query_freq(compress_integer &codex) :
         		query(codex),
           		top_results(accumulator_pointers, top_k)
           		{
             	rewind();
             	}

            /*
				QUERY_FREQ::~QUERY_FREQ()
				-----------------------------
			*/
			/*!
				@brief Destructor
			*/
            virtual ~query_freq()
            	{
             	/* Nothing */
             	}

            /*
				QUERY_FREQ::INIT()
				--------------------
			*/
			/*!
				@brief Initialise the object. MUST be called before first use.
				@param primary_keys [in] Vector of the document primary keys used to convert from internal document ids to external primary keys.
				@param documents [in] The number of documents in the collection.
				@param top_k [in]	The top-k documents to return from the query once executed.
				@param width [in] Not used
			*/
			virtual void init(const std::vector<std::string> &primary_keys, DOCID_TYPE documents = 1024, DOCID_TYPE top_k = 10, size_t width = 7)
				{
				query::init(primary_keys, documents, top_k);
				::memset(accumulator,0,documents * sizeof(*accumulator));
				top_results.set_top_k(top_k);
				::memset(score_freq,0,MAX_RSV * sizeof(*score_freq));
				score_freq[0] = documents; //all documents start with a score of 0
				}

			/*
				QUERY_FREQ::GET_FIRST()
				----------------------------
			*/
			/*!
				@brief Return the top result.
				@return The first (i.e. top) result in the results list.
			*/
			virtual docid_rsv_pair *get_first(void)
				{
				sort();
				next_result_location = 0;
				return get_next();
				}

			/*
				QUERY_FREQ::GET_NEXT()
				------------------------
			*/
			/*!
				@brief After calling get_first(), return the next result
				@return The next result in the results list, or NULL if at end of list
			*/
			virtual docid_rsv_pair *get_next(void)
				{
				if (next_result_location >= top_k - needed_for_top_k)
					return NULL;

				size_t id = accumulator_pointers[top_k - next_result_location - 1].pointer() - &accumulator[0];
				next_result.document_id = id;
				next_result.primary_key = &((*primary_keys)[id]);
				next_result.rsv = accumulator[id];

				next_result_location++;

				return &next_result;
				}


			/*
				QUERY_FREQ::REWIND()
				----------------------
			*/
			/*!
				@brief Clear this object after use and ready for re-use in a new query
			*/
			virtual void rewind(ACCUMULATOR_TYPE smallest_possible_rsv = 0, ACCUMULATOR_TYPE top_k_lower_bound = 1, ACCUMULATOR_TYPE largest_possible_rsv = 0)
				{
				sorted = false;
				query::rewind(largest_possible_rsv);
				::memset(accumulator, 0, documents * sizeof(*accumulator));
				::memset(score_freq,0,MAX_RSV * sizeof(*score_freq));
				score_freq[0] = documents; //all documents start with a score of 0
				needed_for_top_k = this->top_k;
				}

			/*
				QUERY_FREQ::SORT()
				--------------------
			*/
			/*!
				@brief sort this results list before iteration over it.
			*/
			virtual void sort(void)
				{
					//todo
					//need to populate the heap based on the score_freq array
					//then sort with top_k_qsort
					//this is similar to query_block_max.h
					if (!sorted)
					{
					ACCUMULATOR_TYPE *current_accumulator = accumulator;
					while (needed_for_top_k > 0)
						{
							accumulator_pointers[--needed_for_top_k] = current_accumulator++;
						}
					top_results.make_heap();

	    			top_k_qsort::sort(accumulator_pointers + needed_for_top_k, top_k - needed_for_top_k, top_k);
					sorted = true;
					}
				}

			/*
				QUERY_FREQ::ADD_RSV()
				-----------------------
			*/
			/*!
				@brief Add weight to the rsv for document document_id
				@param document_id [in] which document to increment
				@param score [in] the amount of weight to add
			*/
			forceinline void add_rsv(DOCID_TYPE document_id, ACCUMULATOR_TYPE score)
				{
				score_freq[accumulator[document_id]]--;
				accumulator[document_id] += score;
				score_freq[accumulator[document_id]]++;
				}

			/*
				QUERY_FREQ::DECODE_WITH_WRITER()
				----------------------------------
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
					query_freq *query_object = new query_freq(codex);
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

					puts("query_freq::PASSED");
					}

     	};
    }
