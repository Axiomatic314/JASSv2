#pragma once

#include <stdlib.h>
#include <stdint.h>

#include <string>
#include <iostream>

namespace JASS
{
    class query_heap_timer
    {
        public:
            size_t time;
            std::string name;
        public:
            query_heap_timer(std::string name): 
                time(0), 
                name(name)
                {
                }

            ~query_heap_timer()
                {
                std::cout << name << " Time:" << time << " ns\n";
                }

            void init(void)
                {
                time = 0;
                }

            void add_time(size_t val)
                {
                time += val;
                }
    };
}