// Copyright (C) 2010  Davis E. King (davis@dlib.net)
// License: Boost Software License   See LICENSE.txt for the full license.
#ifndef DLIB_LINEAR_MANIFOLD_ReGULARIZER_H__
#define DLIB_LINEAR_MANIFOLD_ReGULARIZER_H__

#include "linear_manifold_regularizer.h"
#include <limits>
#include <vector>
#include "../serialize.h"
#include "../matrix.h"

namespace dlib
{
    namespace impl
    {
        class undirected_adjacency_list
        {
            /*!
                WHAT THIS OBJECT REPRESENTS
                    This object is simply a tool for turning a vector of sample_pair objects
                    into an adjacency list with floating point weights on each edge.  
            !*/
        public:

            undirected_adjacency_list (
            )
            {
                _size = 0;
            }

            struct neighbor 
            {
                neighbor(unsigned long idx, float w):index(idx), weight(w) {}
                neighbor():index(0), weight(0) {}

                unsigned long index;
                float weight;
            };

            typedef std::vector<neighbor>::const_iterator const_iterator;

            unsigned long size (
            ) const
            {
                return _size;
            }

            const_iterator begin(
                unsigned long idx
            ) const
            /*!
                requires
                    - idx < size()
            !*/
            {
                return blocks[idx];
            }

            const_iterator end(
                unsigned long idx
            ) const
            /*!
                requires
                    - idx < size()
            !*/
            {
                return blocks[idx+1];
            }


            template <typename vector_type, typename weight_function_type>
            void build (
                const vector_type& edges,
                const weight_function_type& weight_funct
            ) 
            /*!
                requires
                    - vector_type == a type with an interface compatible with std::vector and 
                      it must in turn contain objects with an interface compatible with dlib::sample_pair
                    - edges.size() > 0
                    - all the elements of edges are unique.  That is:
                        - for all valid i and j where i != j:
                          it must be true that edges[i] != edges[j]
                    - weight_funct(edges[i]) must be a valid expression that evaluates to a
                      floating point number
            !*/
            {


                // Figure out how many neighbors each sample ultimately has.  We do this so 
                // we will know how much space to allocate in the data vector.
                std::vector<unsigned long> num_neighbors;
                num_neighbors.reserve(edges.size());

                for (unsigned long i = 0; i < edges.size(); ++i)
                {
                    // make sure num_neighbors is always big enough 
                    const unsigned long min_size = std::max(edges[i].index1(), edges[i].index2())+1;
                    if (num_neighbors.size() < min_size)
                        num_neighbors.resize(min_size,  0);

                    num_neighbors[edges[i].index1()] += 1;
                    num_neighbors[edges[i].index2()] += 1;
                }

                _size = num_neighbors.size();

                // Now setup the iterators in blocks.  Also setup a version of blocks that holds
                // non-const iterators so we can use it below when we populate data.
                std::vector<std::vector<neighbor>::iterator> mutable_blocks;
                data.resize(edges.size()*2); // each edge will show up twice 
                blocks.resize(_size + 1);
                blocks[0] = data.begin();
                mutable_blocks.resize(_size + 1);
                mutable_blocks[0] = data.begin();
                for (unsigned long i = 0; i < num_neighbors.size(); ++i)
                {
                    blocks[i+1]         = blocks[i]         + num_neighbors[i];
                    mutable_blocks[i+1] = mutable_blocks[i] + num_neighbors[i];
                }

                // finally, put the edges into data
                for (unsigned long i = 0; i < edges.size(); ++i)
                {
                    const float weight = weight_funct(edges[i]);
                    *mutable_blocks[edges[i].index1()]++ = neighbor(edges[i].index2(), weight);
                    *mutable_blocks[edges[i].index2()]++ = neighbor(edges[i].index1(), weight);
                }

            }

        private:

            /*!
                INITIAL VALUE
                    - _size == 0
                    - data.size() == 0
                    - blocks.size() == 0

                CONVENTION
                    - size() == _size
                    - blocks.size() == _size + 1
                    - blocks == a vector of iterators that point into data.  
                      For all valid i:
                        - The iterator range [blocks[i], blocks[i+1]) contains all the edges
                          for the i'th node in the graph
            !*/

            std::vector<neighbor> data;
            std::vector<const_iterator> blocks; 
            unsigned long _size;
        };

    }

// ----------------------------------------------------------------------------------------

    template <
        typename matrix_type
        >
    class linear_manifold_regularizer
    {
        /*!
            REQUIREMENTS ON matrix_type
                Must be some type of dlib::matrix.

            WHAT THIS OBJECT REPRESENTS
                This object computes the inv(T) matrix described in the following paper:
                    Linear Manifold Regularization for Large Scale Semi-supervised Learning
                    by Vikas Sindhwani, Partha Niyogi, and Mikhail Belkin
        !*/

    public:
        typedef typename matrix_type::mem_manager_type mem_manager_type;
        typedef typename matrix_type::type scalar_type;
        typedef typename matrix_type::layout_type layout_type;
        typedef matrix<scalar_type,0,0,mem_manager_type,layout_type> general_matrix;


        template <
            typename vector_type1, 
            typename vector_type2, 
            typename weight_function_type
            >
        void build (
            const vector_type1& samples,
            const vector_type2& edges,
            const weight_function_type& weight_funct
        )
        {
            impl::undirected_adjacency_list graph;
            graph.build(edges, weight_funct);

            make_mr_matrix(samples, graph);
        }

        general_matrix get_transformation_matrix (
            scalar_type intrinsic_regularization_strength
        ) const
        /*!
            requires
                - intrinsic_regularization_strength >= 0
        !*/
        {
            return inv_lower_triangular(chol(identity_matrix<scalar_type>(reg_mat.nr()) + intrinsic_regularization_strength*reg_mat));
        }

    private:

        template <typename vector_type>
        void make_mr_matrix (
            const vector_type& samples,
            const impl::undirected_adjacency_list& graph
        )
        /*!
            requires
                - samples.size() == graph.size()
        !*/
        {
            const unsigned long dims = samples[0].size();
            reg_mat.set_size(dims,dims);
            reg_mat = 0;

            // Compute trans(X)*lap(graph)*X where X is the data matrix 
            // (i.e. the matrix that contains all the samples in its rows)
            // and lap(graph) is the laplacian matrix of the graph.

            typename impl::undirected_adjacency_list::const_iterator beg, end;

            // loop over the columns of the X matrix
            for (unsigned long d = 0; d < dims; ++d)
            {
                // loop down the row of X
                for (unsigned long i = 0; i < graph.size(); ++i)
                {
                    beg = graph.begin(i);
                    end = graph.end(i);

                    // if this node in the graph has any neighbors
                    if (beg != end)
                    {
                        double weight_sum = 0;
                        double val = 0;
                        for (; beg != end; ++beg)
                        {
                            val -= beg->weight * samples[beg->index](d);
                            weight_sum += beg->weight;
                        }
                        val += weight_sum * samples[i](d);

                        for (unsigned long j = 0; j < dims; ++j)
                        {
                            reg_mat(d,j) += val*samples[i](j);
                        }
                    }
                }
            }

        }

        general_matrix reg_mat;
    };

}

// ----------------------------------------------------------------------------------------

#endif // DLIB_LINEAR_MANIFOLD_ReGULARIZER_H__

