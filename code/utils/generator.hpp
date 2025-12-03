#pragma once

#include <cstdlib>
#include <vector>
#include <random>
#include "allocator.hpp"

#include <algorithm>

enum GenerationType{
    UNIFORM,        //generative
    BASIC_UNIFORM,  //generative
    INCREASING,     // generative
    ONE,            // generative
    ID,             // generative
    SHUFFEL         // transformative
};
class Datagenerator{
public:

    Datagenerator(const size_t seed) : _seed(seed), _original_seed(seed){}
    Datagenerator(){
        std::random_device rd;
        _original_seed = rd();
        _seed = _original_seed;
    }

    template<typename T>
    std::size_t generate(T* data, std::size_t count, GenerationType type, T min_value, T max_value){
        _seed++;

        std::mt19937 g(_seed);
        std::uniform_int_distribution<T> uni_form_dist(min_value, max_value - 1);

        const T dif = static_cast<T>(max_value) - static_cast<T>(min_value);
        switch(type){
            case UNIFORM:
                std::generate(data, data + count, [&] () {return uni_form_dist(g);});
                break;
            case BASIC_UNIFORM:
                #pragma omp parallel for
                for(size_t i = 0; i < count; i++){
                    data[i] = (i % dif) + min_value;
                }
                std::shuffle(data, data + count, g);
                break;
            case INCREASING:
                #pragma omp parallel for
                for(size_t i = 0; i < count; i++){
                    data[i] = i + min_value;
                }
                break;
            case ONE:
                #pragma omp parallel for
                for(size_t i = 0; i < count; i++){
                    data[i] = 1;
                }
                break;
            case ID:
                #pragma omp parallel for
                for(size_t i = 0; i < count; i++){
                    data[i] = i;
                }
                break;
            case SHUFFEL:
                std::shuffle(data, data + count, g);
                break;
        }
        return _seed;
    }

    template<typename T>
    std::size_t generate(T* data, std::size_t count, GenerationType type, T max_value){
        return generate<T>(data, count, type, 0, max_value);
    }

    template<typename T>
    std::size_t generate(T* data, std::size_t count, GenerationType type){
        return generate<T>(data, count, type, _min_value, _max_value);
    }

    void set_min_max(std::size_t min_value, std::size_t max_value){
        _min_value = min_value;
        _max_value = max_value;
    }

private:
    std::size_t _min_value = 0;
    std::size_t _max_value = 100;
    std::size_t _seed = 0;
    std::size_t _original_seed = 0;
};

// combining two or more columns into one. Transforming from SOA to AOS
template<typename T>
void combine_columns(T* data, std::vector<T*> raw, std::size_t count){
    const size_t columns = raw.size();
    #pragma omp parallel for
    for(size_t i = 0; i < count; i++){
        for(size_t c = 0; c < columns; c++){
            data[i * columns + c] = raw[c][i];
        }
    }
    print_page_info(data, count * columns);
}