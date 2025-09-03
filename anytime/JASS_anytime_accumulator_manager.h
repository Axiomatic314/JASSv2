/*
	JASS_ANYTIME_ACCUMULATOR_MANAGER.H
	----------------------------------
	Copyright (c) 2025 Andrew Trotman
	Released under the 2-clause BSD license (See:https://en.wikipedia.org/wiki/BSD_licenses)
*/
/*!
	@file
	@brief Facrory class for accumulator management strategies
	@author Andrew Trotman
	@copyright 2025 Andrew Trotman
*/
#pragma once

#include <string>

#include "query_heap.h"
#include "query_simple.h"
#include "compress_integer.h"

namespace JASS_anytime_accumulator_manager
	{
	/*
		GET_BY_NAME()
		-------------
	*/
	/*!
		@brief Return an accumulator manager given its name, which will normally come from the command line parameter parsing
		@param name [in] The name of the manager to use
		@param codex [in] The decompressor that the manager should use
		@return An accumulator manager
	*/
	JASS::query *get_by_name(const std::string &name, JASS::compress_integer &codex)
		{
		std::cout << "ACCUMULATOR MANAGER:" << name << "\n";

		if (name == "2d_heap")
			return new JASS::query_heap(codex);
		else if (name == "simple")
			return new JASS::query_simple(codex);
		else
			{
			std::cout << "ACCUMULATOR MANAGER IS UNKNONW! USING 2d_heap\n";
			return new JASS::query_heap(codex);
			}
		}
	}
