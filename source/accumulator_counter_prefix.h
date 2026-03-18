/*
	ACCUMULTOR_COUNTER_PREFIX.H
	-------------------
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
		CLASS ACCUMULATOR_COUNTER_PREFIX
		------------------------
	*/
	/*!
		@brief Store the accumulators in an array and use a query counter to know when to clear.
		@tparam ELEMENT The type of accumulator being used (default is uint16_t)
		@tparam NUMBER_OF_ACCUMULATORS The maxium number of documents allowed in any index
	*/
	template <typename ELEMENT, size_t NUMBER_OF_ACCUMULATORS, typename = typename std::enable_if<std::is_arithmetic<ELEMENT>::value, ELEMENT>::type>
	class accumulator_counter_prefix
		{
		/*
			This somewhat bizarre line is so that unittest() can see the private members of another instance of the class.
		*/
		template<typename A, size_t B, typename C> friend class accumulator_counter_prefix;

		private:
			ELEMENT accumulator[NUMBER_OF_ACCUMULATORS];				///< The accumulator array
			size_t number_of_accumulators;								///< The number of accumulators that the user asked for
            uint8_t query_counter; 										///<
            uint8_t prefix; 												///< The width of the query counter prefix
            ELEMENT width; 												///< The width of the accumulator value

		public:
			/*
				ACCUMULATOR_COUNTER_PREFIX::ACCUMULATOR_COUNTER_PREFIX()
				----------------------------------------
			*/
			/*!
				@brief Constructor.
			*/
			accumulator_counter_prefix() :
				number_of_accumulators(0),
                query_counter(0)
				{
				/* Nothing */
				}

			/*
				ACCUMULATOR_COUNTER_PREFIX::~ACCUMULATOR_COUNTER_PREFIX()
				-----------------------------------------
			*/
			/*!
				@brief Destructor.
			*/
			virtual ~accumulator_counter_prefix()
				{
				/* Nothing */
				}

			/*
				ACCUMULATOR_COUNTER_PREFIX::INIT()
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
                prefix = preferred_width; //todo: change the param used for this
                width = sizeof(ELEMENT) * 8 - prefix;
				rewind();
				query_counter = 0;
				}

			/*
				ACCUMULATOR_COUNTER_PREFIX::GET_VALUE()
				-------------------------------
			*/
			/*!
				@brief Return the value of the given accumulator
                @details This interface does not initialise an accumulator, it returns 0 if the accumulator is uninitialised
				@param which [in] The accumulator to return.
				@return The accumulator value or 0.
			*/
			forceinline ELEMENT get_value(size_t which)
				{
                if ((accumulator[which] >> width) != query_counter)
                    return 0;
                else
				    return accumulator[which] & ((1 << width) - 1);
				}

			/*
				ACCUMULATOR_COUNTER_PREFIX::OPERATOR[]()
				--------------------------------
			*/
			/*!
				@brief Return a reference to the given accumulator
				@details The only valid way to access the accumulators is through this interface. It ensures the accumulator
                has been initialised before the first time it is returned to the caller.
				@param which [in] The accumulator to return.
				@return The accumulator.
			*/
			forceinline ELEMENT &operator[](size_t which)
				{
				if ((accumulator[which] >> width) != query_counter)
                    accumulator[which] = query_counter << width;
                return accumulator[which];
				}

			/*
				ACCUMULATOR_COUNTER_PREFIX::GET_INDEX()
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
				ACCUMULATOR_COUNTER_PREFIX::SIZE()
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
				ACCUMULATOR_COUNTER_PREFIX::REWIND()
				----------------------------
			*/
			/*!
				@brief Clear the accumulators ready for use
				@details This only clears the accumulator array if the query counter has hit 0.
			*/
			void rewind(void)
				{
                if (query_counter == 0)
				    ::memset(accumulator, 0, number_of_accumulators * sizeof(*accumulator));
                query_counter = (query_counter + 1) % (1 << prefix);
				}

			/*
				ACCUMULATOR_COUNTER_PREFIX::UNITTEST_EXAMPLE()
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
				ACCUMULATOR_COUNTER_PREFIX::UNITTEST()
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
				accumulator_counter_prefix<size_t, 64> array;
				array.init(64);
				unittest_example(array);

				/*
					Make sure it all works right when there is a single accumulator
				*/
				accumulator_counter_prefix<size_t, 1> array_one;
				array_one.init(1);
				unittest_example(array_one);

				puts("accumulator_counter_prefix::PASSED");
				}
		};
	}
