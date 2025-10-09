/*
	ACCUMULTOR_BLOCK_MAX.H
	----------------------
	Copyright (c) 2025 Andrew Trotman
	Released under the 2-clause BSD license (See:https://en.wikipedia.org/wiki/BSD_licenses)
*/
/*!
	@file
	@brief Store the accumulators in a block-max array as originally used in IOQP.
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

#include "maths.h"
#include "forceinline.h"

namespace JASS
	{
	/*
		CLASS ACCUMULATOR_BLOCK_MAX
		---------------------------
	*/
	/*!
		@brief Store the accumulators in a block-max array as originally used in IOQP.
		@tparam ELEMENT The type of accumulator being used (default is uint16_t)
		@tparam NUMBER_OF_ACCUMULATORS The maxium number of documents allowed in any index
	*/
	template <typename ELEMENT, size_t NUMBER_OF_ACCUMULATORS, typename = typename std::enable_if<std::is_arithmetic<ELEMENT>::value, ELEMENT>::type>
	class accumulator_block_max
		{
		/*
			This somewhat bizar line is so that unittest() can see the private members of another instance of the class.
		*/
		template<typename A, size_t B, typename C> friend class accumulator_block_max;

		private:
			static constexpr size_t maximum_number_of_blocks = NUMBER_OF_ACCUMULATORS;
			static constexpr size_t maximum_number_of_accumulators_allocated = NUMBER_OF_ACCUMULATORS + NUMBER_OF_ACCUMULATORS / 2;			///< The numner of accumulators that were actually allocated (so that the last block is a full block)

		public:
			alignas(__m512i) ELEMENT block_max[maximum_number_of_blocks];																						///< The dirty flags are kept as bytes for faster lookup
			alignas(__m512i) ELEMENT accumulator[maximum_number_of_accumulators_allocated];																///< The accumulators are kept in an array

			uint32_t shift;											///< The amount to shift to get the right dirty flag
		public:
			size_t width;												///< Each dirty flag represents this number of accumulators in a "row"
			size_t number_of_blocks;								///< The number of "rows" (i.e. dirty flags)
		private:
			size_t number_of_accumulators_allocated;			///< The numner of accumulators that were actually allocated (recall that this is a 2D array)
			size_t number_of_accumulators;						///< The number of accumulators that the user asked for

		public:
			/*
				ACCUMULATOR_BLOCK_MAX::ACCUMULATOR_BLOCK_MAX()
				----------------------------------------------
			*/
			/*!
				@brief Constructor.
			*/
			accumulator_block_max() :
				shift(1),
				width(1),
				number_of_blocks(0),
				number_of_accumulators_allocated(0),
				number_of_accumulators(0)
				{
				/* Nothing */
				}

			/*
				ACCUMULATOR_BLOCK_MAX::~ACCUMULATOR_BLOCK_MAX()
				-----------------------------------------------
			*/
			/*!
				@brief Destructor.
			*/
			virtual ~accumulator_block_max()
				{
				/* Nothing */
				}

			/*
				ACCUMULATOR_BLOCK_MAX::INIT()
				-----------------------------
			*/
			/*!
				@brief Initialise this object before first use.
				@param number_of_accumulators [in] The numnber of elements in the array being managed.
				@param preferred_width [in] The preferred width of each block, where the actual width is 2^preferred_width (if possible)
			*/
			void init(size_t number_of_accumulators, size_t preferred_width = 0)
				{
				this->number_of_accumulators = number_of_accumulators;
				/*
					If the width of the accumulator array is a whole power of 2 the its quick to find the block.  If the width is the square root of the
					number of accumulators then it ballances the number of accumulator with the number of blocks.  Both techniques are used.
				*/
				if (preferred_width >= 1)
					shift = (uint32_t)preferred_width;
				else
					shift = (uint32_t)maths::floor_log2((size_t)sqrt(number_of_accumulators));

				width = (size_t)1 << shift;

				/*
					Round up the number of dirty flags so that if the number of accumulators isn't a square that we don't miss the last row
				*/
				number_of_blocks = (number_of_accumulators + width - 1) / width;

				/*
					Round up the number of accumulators to make a rectangle so scanning is faster
				*/
				number_of_accumulators_allocated = width * number_of_blocks;

				/*
					Check we've not gone past the end of the arrays
				*/
				if (number_of_blocks > maximum_number_of_blocks || number_of_accumulators_allocated > maximum_number_of_accumulators_allocated)
					throw std::bad_array_new_length();

				/*
					Clear the dirty flags ready for first use.
				*/
				rewind();

				/*
					Since the accumulator array is guaranteed to have enough that each block is "full" (has an accumulator "row" behind it),
					there are more accumulators allocated (and accessed) then documents in the collections, so zero the ones that can't get
					touched here in init() rather than in rewind()
				*/
				::memset(accumulator + number_of_accumulators, 0, (number_of_accumulators_allocated - number_of_accumulators) * sizeof(*accumulator));
				}

			/*
				ACCUMULATOR_BLOCK_MAX::WHICH_BLOCK()
				------------------------------------
			*/
			/*!
				@brief Return the id of the block to use.
				@param element [in] The accumulator number.
				@return The block number.
			*/
			forceinline size_t which_block(size_t element) const
				{
				return element >> shift;
				}

			/*
				ACCUMULATOR_BLOCK_MAX::ADD()
				----------------------------
			*/
			/*!
				@brief add value to a given accumulator.  i.e. accumulator[which] += value;
				@param which [in] Which accumulator to add to
				@param value [in] The value to add
			*/
			forceinline void add(size_t which, ELEMENT value)
				{
				accumulator[which] += value;

				if (accumulator[which] > block_max[which_block(which)])
					block_max[which_block(which)] = accumulator[which];
				}

			/*
				ACCUMULATOR_BLOCK_MAX::OPERATOR[]()
				-----------------------------------
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
				return accumulator[which];
				}

			/*
				ACCUMULATOR_BLOCK_MAX::GET_INDEX()
				----------------------------------
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
				ACCUMULATOR_BLOCK_MAX::SIZE()
				-----------------------------
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
				ACCUMULATOR_BLOCK_MAX::REWIND()
				-------------------------------
			*/
			/*!
				@brief Clear the accumulators ready for use
			*/
			void rewind(void)
				{
				/*
					Initialise the accumulators then initialise the block_max array
				*/
				::memset(accumulator, 0, number_of_accumulators * sizeof(*accumulator));
				::memset(block_max, 0, number_of_blocks * sizeof(*block_max));
				}

			/*
				ACCUMULATOR_BLOCK_MAX::UNITTEST_EXAMPLE()
				-----------------------------------------
			*/
			/*!
				@brief Unit test a single 2D accumulator instance making sure its correct
			*/
			template <typename ACCUMULATOR_TYPE>
			static void unittest_example(ACCUMULATOR_TYPE &instance)
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
				ACCUMULATOR_BLOCK_MAX::UNITTEST()
				---------------------------------
			*/
			/*!
				@brief Unit test this class
			*/
			static void unittest(void)
				{
				/*
					Allocate an array of 64 accumulators and make sure the width and height are correct
				*/
				accumulator_block_max<size_t, 64> array;
				array.init(64);
				JASS_assert(array.width == 8);
				JASS_assert(array.shift == 3);
				JASS_assert(array.number_of_blocks == 8);

				unittest_example(array);

				/*
					Make sure it all works right when there is a single accumulator in the last row
				*/
				accumulator_block_max<size_t, 65> array_hangover;
				array_hangover.init(65);
				JASS_assert(array_hangover.width == 8);
				JASS_assert(array_hangover.shift == 3);
				JASS_assert(array_hangover.number_of_blocks == 9);

				unittest_example(array_hangover);

				/*
					Make sure it all works right when there is a single accumulator missing from the last row
				*/
				accumulator_block_max<size_t, 63> array_hangunder;
				array_hangunder.init(63);
				JASS_assert(array_hangunder.width == 4);
				JASS_assert(array_hangunder.shift == 2);
				JASS_assert(array_hangunder.number_of_blocks == 16);

				unittest_example(array_hangunder);

				/*
					Make sure it all works right when there is a single accumulator
				*/
				accumulator_block_max<size_t, 1> array_one;
				array_one.init(1);
				JASS_assert(array_one.width == 1);
				JASS_assert(array_one.shift == 0);
				JASS_assert(array_one.number_of_blocks == 1);

				unittest_example(array_one);

				puts("accumulator_block_max::PASSED");
				}
		};
	}
