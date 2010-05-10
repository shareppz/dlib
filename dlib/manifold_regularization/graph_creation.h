// Copyright (C) 2010  Davis E. King (davis@dlib.net)
// License: Boost Software License   See LICENSE.txt for the full license.
#ifndef DLIB_GRAPH_CrEATION_H__
#define DLIB_GRAPH_CrEATION_H__

#include "graph_creation_abstract.h"
#include <limits>
#include <vector>
#include "../string.h"
#include "../rand.h"
#include <algorithm>

namespace dlib
{

// ----------------------------------------------------------------------------------------

    namespace impl
    {
        template <typename iterator>
        iterator iterator_of_worst (
            iterator begin,
            const iterator& end
        ) 
        /*!
            ensures
                - returns an iterator that points to the element in the given range that has the biggest
                  distance 
        !*/
        {
            float dist = begin->distance();
            iterator worst = begin;
            for (; begin != end; ++begin)
            {
                if (begin->distance() > dist)
                {
                    dist = begin->distance();
                    worst = begin;
                }
            }

            return worst;
        }

    }

// ----------------------------------------------------------------------------------------

    template <
        typename vector_type,
        typename distance_function_type,
        typename alloc,
        typename T
        >
    void find_percent_shortest_edges_randomly (
        const vector_type& samples,
        const distance_function_type& dist_funct,
        const double percent,
        const unsigned long num,
        const T& random_seed,
        std::vector<sample_pair, alloc>& out
    )
    /*!
        requires
            - samples.size() > 1
        - 0 < percent <= 1
        - num > 0
        - random_seed must be convertible to a string by dlib::cast_to_string()
    !*/
    {
        std::vector<sample_pair, alloc> edges;
        edges.reserve(num);

        dlib::rand::kernel_1a rnd;
        rnd.set_seed(cast_to_string(random_seed));

        // randomly sample a bunch of edges
        while (edges.size() < num)
        {
            const unsigned long idx1 = rnd.get_random_32bit_number()%samples.size();
            const unsigned long idx2 = rnd.get_random_32bit_number()%samples.size();
            if (idx1 != idx2)
            {
                edges.push_back(sample_pair(idx1, idx2, dist_funct(samples[idx1], samples[idx2])));
            }
        }


        // sort the edges so that duplicate edges will be adjacent
        std::sort(edges.begin(), edges.end(), &order_by_index);

        // now put edges into out while avoiding duplicates
        out.clear();
        out.reserve(edges.size());
        out.push_back(edges[0]);
        for (unsigned long i = 1; i < edges.size(); ++i)
        {
            if (edges[i] != edges[i-1])
            {
                out.push_back(edges[i]);
            }
        }


        // now sort all the edges by distance and take the percent with the smallest distance
        std::sort(out.begin(), out.end(), &order_by_distance);
        out.swap(edges);
        out.assign(edges.begin(), edges.begin() + edges.size()*percent);
    }

// ----------------------------------------------------------------------------------------

    template <
        typename vector_type,
        typename distance_function_type,
        typename alloc
        >
    void find_k_nearest_neighbors (
        const vector_type& samples,
        const distance_function_type& dist_funct,
        const unsigned long k,
        std::vector<sample_pair, alloc>& out
    )
    /*!
        requires
            - samples.size() > k
        - k > 0
    !*/
    {
        using namespace impl;
        std::vector<sample_pair> edges;

        edges.resize(samples.size()*k);

        std::vector<float> worst_dists(samples.size(), std::numeric_limits<float>::max());

        std::vector<sample_pair>::iterator begin_i, end_i, begin_j, end_j, itr;
        begin_i = edges.begin();
        end_i = begin_i + k;

        // Loop over all combinations of samples.   We will maintain the iterator ranges so that
        // within the inner for loop we have:
        //   [begin_i, end_i) == the range in edges that contains neighbors of samples[i]
        //   [begin_j, end_j) == the range in edges that contains neighbors of samples[j]
        for (unsigned long i = 0; i < samples.size(); ++i)
        {
            begin_j = begin_i + 1;
            end_j = begin_j + k;

            for (unsigned long j = i+1; j < samples.size(); ++j)
            {
                const float dist = dist_funct(samples[i], samples[j]);

                if (dist < worst_dists[i])
                {
                    *iterator_of_worst(begin_i, end_i) = sample_pair(i, j, dist);
                    worst_dists[i] = iterator_of_worst(begin_i, end_i)->distance();
                }

                if (dist < worst_dists[j])
                {
                    *iterator_of_worst(begin_j, end_j) = sample_pair(i, j, dist);
                    worst_dists[j] = iterator_of_worst(begin_j, end_j)->distance();
                }

                begin_j += k;
                end_j += k;
            }

            begin_i += k;
            end_i += k;
        }

        // sort the edges so that duplicate edges will be adjacent
        std::sort(edges.begin(), edges.end(), &order_by_index);

        // now put edges into out while avoiding duplicates
        out.clear();
        out.reserve(edges.size());
        out.push_back(edges[0]);
        for (unsigned long i = 1; i < edges.size(); ++i)
        {
            if (edges[i] != edges[i-1])
            {
                out.push_back(edges[i]);
            }
        }

    }

// ----------------------------------------------------------------------------------------

}

#endif // DLIB_GRAPH_CrEATION_H__


