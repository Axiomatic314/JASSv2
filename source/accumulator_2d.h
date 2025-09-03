/*
	ACCUMULTOR_2D.H
	---------------
	Copyright (c) 2017 Andrew Trotman
	Released under the 2-clause BSD license (See:https://en.wikipedia.org/wiki/BSD_licenses)
*/
/*!
	@file
	@brief Store the accumulators in a 2D array as originally used in ATIRE.
	@author Andrew Trotman
	@copyright 2017 Andrew Trotman
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
		CLASS ACCUMULATOR_2D
		--------------------
	*/
	/*!
		@brief Store the accumulators in a 2D array as originally used in ATIRE.
		@details Manage the accumulagtor array as a two dimensional array.  This approach avoids initialising the accumulators on each search by
		breakig it into a series of pages and keeping clean flag for each page.  A page of accmumulators is only initialised if one of the elements
		in that page is touched.  This detail is kept in a set of flags (one per page) known as the dirty flags.  The details are described in
		X.-F. Jia, A. Trotman, R. O'Keefe (2010), Efficient Accumulator Initialisation, Proceedings of the 15th Australasian Document Computing Symposium (ADCS 2010).
		This implementation differs from that implenentation is so far as the size of the page is alwaya a whole power of 2 and thus the dirty flag can
		be found with a bit shift rather than a mod.  It also uses dirty flags rather than clean flags as it requires one fewer instruction to check
		@tparam ELEMENT The type of accumulator being used (default is uint16_t)
		@tparam NUMBER_OF_ACCUMULATORS The maxium number of documents allowed in any index
*/
	template <typename ELEMENT, size_t NUMBER_OF_ACCUMULATORS, typename = typename std::enable_if<std::is_arithmetic<ELEMENT>::value, ELEMENT>::type>
	class accumulator_2d
		{
		/*
			This somewhat bizar line is so that unittest() can see the private members of another instance of the class.
		*/
		template<typename A, size_t B, typename C> friend class accumulator_2d;

		private:
			typedef uint8_t flag_type;

		private:
			static constexpr size_t maximum_number_of_dirty_flags = NUMBER_OF_ACCUMULATORS;
			static constexpr size_t maximum_number_of_accumulators_allocated = NUMBER_OF_ACCUMULATORS + NUMBER_OF_ACCUMULATORS / 2;			///< The numner of accumulators that were actually allocated (recall that this is a 2D array)

		private:
			alignas(__m512i) flag_type dirty_flag[maximum_number_of_dirty_flags];																							///< The dirty flags are kept as bytes for faster lookup
			alignas(__m512i) ELEMENT accumulator[maximum_number_of_accumulators_allocated];																				///< The accumulators are kept in an array

			size_t width;												///< Each dirty flag represents this number of accumulators in a "row"
			uint32_t shift;											///< The amount to shift to get the right dirty flag
			size_t number_of_dirty_flags;							///< The number of "rows" (i.e. dirty flags)
			size_t number_of_accumulators_allocated;			///< The numner of accumulators that were actually allocated (recall that this is a 2D array)
			size_t number_of_accumulators;						///< The number of accumulators that the user asked for

		public:
			/*
				ACCUMULATOR_2D::ACCUMULATOR_2D()
				--------------------------------
			*/
			/*!
				@brief Constructor.
			*/
			accumulator_2d() :
				width(1),
				shift(1),
				number_of_dirty_flags(0),
				number_of_accumulators_allocated(0),
				number_of_accumulators(0)
				{
				/* Nothing */
				}

			/*
				ACCUMULATOR_2D::~ACCUMULATOR_2D()
				---------------------------------
			*/
			/*!
				@brief Destructor.
			*/
			virtual ~accumulator_2d()
				{
				}

			/*
				ACCUMULATOR_2D::INIT()
				----------------------
			*/
			/*!
				@brief Initialise this object before first use.
				@param number_of_accumulators [in] The numnber of elements in the array being managed.
				@param preferred_width [in] The preferred width of each "page" in the page table, where the actual width is 2^preferred_width (if possible)
			*/
			void init(size_t number_of_accumulators, size_t preferred_width = 0)
				{
				this->number_of_accumulators = number_of_accumulators;
				/*
					If the width of the accumulator array is a whole power of 2 the its quick to find the dirty flag.  If the width is the square root of the
					number of accumulators then it ballances the number of accumulator with the number of dirty flags.  Both techniques are used.
				*/
				if (preferred_width >= 1)
					shift = (uint32_t)preferred_width;
				else
					shift = (uint32_t)maths::floor_log2((size_t)sqrt(number_of_accumulators));

				width = (size_t)1 << shift;

				/*
					Round up the number of dirty flags so that if the number of accumulators isn't a square that we don't miss the last row
				*/
				number_of_dirty_flags = (number_of_accumulators + width - 1) / width;

				/*
					Round up the number of accumulators to make a rectangle (we're a 2D array)
				*/
				number_of_accumulators_allocated = width * number_of_dirty_flags;

				/*
					Check we've not gone past the end of the arrays
				*/
				if (number_of_dirty_flags > maximum_number_of_dirty_flags || number_of_accumulators_allocated > maximum_number_of_accumulators_allocated)
					throw std::bad_array_new_length();

				/*
					Clear the dirty flags ready for first use.
				*/
				rewind();
				}

			/*
				ACCUMULATOR_2D::WHICH_DIRTY_FLAG()
				----------------------------------
			*/
			/*!
				@brief Return the id of the dirty flag to use.
				@param element [in] The accumulator number.
				@return The dirty flag number.
			*/
			forceinline size_t which_dirty_flag(size_t element) const
				{
				return element >> shift;
				}

			/*
				ACCUMULATOR_2D::GET_VALUE()
				---------------------------
			*/
			/*!
				@brief Return the value of the given accumulator
				@details This interface does not initialise an accumulator, it returns 0 if the accumulator is uninitialised
				@param which [in] The accumulator to return.
				@return The accumulator value or 0.
			*/
			forceinline ELEMENT get_value(size_t which)
				{
				size_t flag = which_dirty_flag(which);
				if (dirty_flag[flag])
					return 0;
				else
					return accumulator[which];
				}

			/*
				ACCUMULATOR_2D::OPERATOR[]()
				----------------------------
			*/
			/*!
				@brief Return a reference to the given accumulator
				@details The only valid way to access the accumulators is through this interface.  It ensures the accumulator
				has been initialised before the first time it is returned to the caller.
				@param which [in] The accumulator to return.
				@return The accumulator.
			*/
			forceinline ELEMENT &operator[](size_t which)
				{
				size_t flag = which_dirty_flag(which);

				if (dirty_flag[flag])
					{
					memset(&accumulator[0] + flag * width, 0, width * sizeof(accumulator[0]));
					dirty_flag[flag] = 0;
					}

				return accumulator[which];
				}

			/*
				ACCUMULATOR_2D::GET_INDEX()
				---------------------------
			*/
			/*!
				@brief Given a pointer to an accumulator, return the acumulator index
				@param return a value such that get_index(&accumulator[x]) == x
			*/
			forceinline size_t get_index(ELEMENT *pointer)
				{
				return pointer - &accumulator[0];
				}

			/*
				ACCUMULATOR_2D::SIZE()
				----------------------
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
				ACCUMULATOR_2D::REWIND()
				------------------------
			*/
			/*!
				@brief Clear the accumulators ready for use
				@details This sets the dirty flags so that the next time an accumulator is requested it is initialised
				to zero before being returned.
			*/
			void rewind(void)
				{
				::memset(&dirty_flag[0], 0xFF, number_of_dirty_flags * sizeof(dirty_flag[0]));
				}

			/*
				ACCUMULATOR_2D::UNITTEST_EXAMPLE()
				----------------------------------
			*/
			/*!
				@brief Unit test a single 2D accumulator instance making sure its correct
			*/
			template <typename ACCUMULATOR_2D>
			static void unittest_example(ACCUMULATOR_2D &instance)
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
				ACCUMULATOR_2D::UNITTEST()
				--------------------------
			*/
			/*!
				@brief Unit test this class
			*/
			static void unittest(void)
				{
				/*
					Allocate an array of 64 accumulators and make sure the width and height are correct
				*/
				accumulator_2d<size_t, 64> array;
				array.init(64);
				JASS_assert(array.width == 8);
				JASS_assert(array.shift == 3);
				JASS_assert(array.number_of_dirty_flags == 8);

				unittest_example(array);

				/*
					Make sure it all works right when there is a single accumulator in the last row
				*/
				accumulator_2d<size_t, 65> array_hangover;
				array_hangover.init(65);
				JASS_assert(array_hangover.width == 8);
				JASS_assert(array_hangover.shift == 3);
				JASS_assert(array_hangover.number_of_dirty_flags == 9);

				unittest_example(array_hangover);

				/*
					Make sure it all works right when there is a single accumulator missing from the last row
				*/
				accumulator_2d<size_t, 63> array_hangunder;
				array_hangunder.init(63);
				JASS_assert(array_hangunder.width == 4);
				JASS_assert(array_hangunder.shift == 2);
				JASS_assert(array_hangunder.number_of_dirty_flags == 16);

				unittest_example(array_hangunder);

				/*
					Make sure it all works right when there is a single accumulator
				*/
				accumulator_2d<size_t, 1> array_one;
				array_one.init(1);
				JASS_assert(array_one.width == 1);
				JASS_assert(array_one.shift == 0);
				JASS_assert(array_one.number_of_dirty_flags == 1);

				unittest_example(array_one);

				puts("accumulator_2d::PASSED");
				}
		};
	}
