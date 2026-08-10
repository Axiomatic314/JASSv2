/*
	JASS_ANYTIME_RESULT.H
	---------------------
	Copyright (c) 2021 Andrew Trotman
	Released under the 2-clause BSD license (See:https://en.wikipedia.org/wiki/BSD_licenses)
*/
/*!
	@file
	@brief The results of a single query
	@author Andrew Trotman
	@copyright 2021 Andrew Trotman
*/
#pragma once

#include <stdio.h>

#include <string>

/*
	CLASS JASS_ANYTIME_RESULT
	-------------------------
*/
class JASS_anytime_result
	{
	public:
		std::string query_id;				///< The query ID
		std::string query;					///< The query
		std::string results_list;			///< The results list
		size_t postings_processed;			///< The number of postings processed for this query
		size_t search_time_in_ns;			///< The time it took to resolve the query
		size_t time_rewind;
		size_t time_add;
		size_t time_heapify;
		size_t time_heap;
		size_t time_decompress;
		size_t time_sort;
		size_t heap_push_back;
		size_t heap_promote;

	/*
		JASS_ANYTIME_RESULT::JASS_ANYTIME_RESULT()
		------------------------------------------
	*/
	/*!
		@brief Constructor
	*/
	JASS_anytime_result() :
		query_id(),
		query(),
		results_list(),
		postings_processed(0),
		search_time_in_ns(0),
		time_rewind(0),
		time_add(0),
		time_heapify(0),
		time_heap(0),
		time_decompress(0),
		time_sort(0),
		heap_push_back(0),
		heap_promote(0)
		{
		/* Nothing */
		}

	/*
		JASS_ANYTIME_RESULT::JASS_ANYTIME_RESULT()
		------------------------------------------
	*/
	/*!
      @brief Constructor
      @param query_id [in] The query ID
      @param query [in] The query
      @param results_list [in] The results list (normally in TREC format)
      @param postings_processed [in] The numvber of postings processed (that is, <docid, impact> pairs)
      @param search_time_in_ns [in] The time it took to resolve the query
	*/
	JASS_anytime_result(const std::string &query_id, const std::string &query, const std::string &results_list, size_t postings_processed, size_t search_time_in_ns, size_t time_rewind, size_t time_add, size_t time_heapify, size_t time_heap, size_t time_decompress, size_t time_sort, size_t heap_push_back, size_t heap_promote) :
		query_id(query_id),
		query(query),
		results_list(results_list),
		postings_processed(postings_processed),
		search_time_in_ns(search_time_in_ns),
		time_rewind(time_rewind),
		time_add(time_add),
		time_heapify(time_heapify),
		time_heap(time_heap),
		time_decompress(time_decompress),
		time_sort(time_sort),
		heap_push_back(heap_push_back),
		heap_promote(heap_promote)
		{
		/* Nothing */
		}
	};
