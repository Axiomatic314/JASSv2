/*
	ACCUMULTOR_SIMPLE.H
	-------------------
	Copyright (c) 2025 Andrew Trotman
	Released under the 2-clause BSD license (See:https://en.wikipedia.org/wiki/BSD_licenses)
*/
/*!
	@file
	@brief Store the accumulators in a simple array that gets zeroed at the start of each query
	@author Andrew Trotman
	@copyright 2025 Andrew Trotman
*/
#pragma once

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <immintrin.h>

#include <new>
#include <bitset>
#include <vector>
#include <random>
#include <numeric>
#include <algorithm>

#include "simd.h"
#include "maths.h"
#include "forceinline.h"

namespace JASS
	{
	/*
		CLASS ACCUMULATOR_SIMPLE
		------------------------
	*/
	/*!
		@brief Store the accumulators in an array.
		@tparam ELEMENT The type of accumulator being used (default is uint16_t)
		@tparam NUMBER_OF_ACCUMULATORS The maxium number of documents allowed in any index
*/
	template <typename ELEMENT, size_t NUMBER_OF_ACCUMULATORS, typename = typename std::enable_if<std::is_arithmetic<ELEMENT>::value, ELEMENT>::type>
	class accumulator_simple
		{
		/*
			This somewhat bizar line is so that unittest() can see the private members of another instance of the class.
		*/
		template<typename A, size_t B, typename C> friend class accumulator_simple;

		private:
			ELEMENT accumulator[NUMBER_OF_ACCUMULATORS];				///< The accumulator array
			size_t number_of_accumulators;								///< The number of accumulators that the user asked for

		public:
			/*
				ACCUMULATOR_SIMPLE::ACCUMULATOR_SIMPLE()
				----------------------------------------
			*/
			/*!
				@brief Constructor.
			*/
			accumulator_simple() :
				number_of_accumulators(0)
				{
				/* Nothing */
				}

			/*
				ACCUMULATOR_SIMPLE::~ACCUMULATOR_SIMPLE()
				-----------------------------------------
			*/
			/*!
				@brief Destructor.
			*/
			virtual ~accumulator_simple()
				{
				}

			/*
				ACCUMULATOR_SIMPLE::INIT()
				--------------------------
			*/
			/*!
				@brief Initialise this object before first use.
				@param number_of_accumulators [in] The number of elements in the array being managed.
				@param preferred_width [in] Ignored
			*/
			void init(size_t number_of_accumulators, size_t preferred_width = 0)
				{
				this->number_of_accumulators = number_of_accumulators;
				rewind();
				}

			/*
				ACCUMULATOR_SIMPLE::GET_VALUE()
				-------------------------------
			*/
			/*!
				@brief Return the value of the given accumulator
				@param which [in] The accumulator to return.
				@return The accumulator value or 0.
			*/
			forceinline ELEMENT get_value(size_t which)
				{
				return accumulator[which];
				}

			/*
				ACCUMULATOR_SIMPLE::OPERATOR[]()
				--------------------------------
			*/
			/*!
				@brief Return a reference to the given accumulator
				@details The only valid way to access the accumulators is through this interface.
				@param which [in] The accumulator to return.
				@return The accumulator.
			*/
			forceinline ELEMENT &operator[](size_t which)
				{
				return accumulator[which];
				}

			/*
				ACCUMULATOR_SIMPLE::GET_INDEX()
				-------------------------------
			*/
			/*!
				@brief Given a pointer to an accumulator, return the accumulator index
				@param return a value such that get_index(&accumulator[x]) == x
			*/
			forceinline size_t get_index(ELEMENT *pointer)
				{
				return pointer - &accumulator[0];
				}

			/*
				ACCUMULATOR_SIMPLE::SIZE()
				--------------------------
			*/
			/*!
				@brief Return the number of accumulators in the array.
				@details Return the number of accumulators in the array which may be fewer than have been allocated.
				@return Size of the accumulator array.
			*/
			size_t size(void) const
				{
				return number_of_accumulators;
				}

			/*
				ACCUMULATOR_SIMPLE::REWIND()
				----------------------------
			*/
			/*!
				@brief Clear the accumulators ready for use
				@details This sets the dirty flags so that the next time an accumulator is requested it is initialised
				to zero before being returned.
			*/
			void rewind(void)
				{
				::memset(accumulator, 0, number_of_accumulators * sizeof(*accumulator));
				}

			/*
				ACCUMULATOR_SIMPLE::UNITTEST_EXAMPLE()
				--------------------------------------
			*/
			/*!
				@brief Unit test a single accumulator instance making sure it's correct
			*/
			template <typename ACCUMULATOR_MANAGER>
			static void unittest_example(ACCUMULATOR_MANAGER &instance)
				{
				/*
					Populate an array with the shuffled sequence 0..instance.size()
				*/
				std::vector<size_t> sequence(instance.size());
				std::iota(sequence.begin(), sequence.end(), 0);
				std::random_device random_number_generator;
				std::shuffle(sequence.begin(), sequence.end(), std::knuth_b(random_number_generator()));

				/*
					Set elemenets and make sure they're correct
				*/
				for (const auto &position : sequence)
					{
					JASS_assert(instance[position] == 0);
					instance[position] = position;
					JASS_assert(instance[position] == position);
					}

				/*
					Make sure no over-writing happened
				*/
				for (size_t element = 0; element < instance.size(); element++)
					JASS_assert(instance[element] == element);
				}

			/*
				ACCUMULATOR_SIMPLE::UNITTEST()
				------------------------------
			*/
			/*!
				@brief Unit test this class
			*/
			static void unittest(void)
				{
				/*
					Allocate an array of 64 accumulators
				*/
				accumulator_simple<size_t, 64> array;
				array.init(64);
				unittest_example(array);

				/*
					Make sure it all works right when there is a single accumulator
				*/
				accumulator_simple<size_t, 1> array_one;
				array_one.init(1);
				unittest_example(array_one);

				puts("accumulator_simple::PASSED");
				}
		};
	}
