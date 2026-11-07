/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    math.hpp: Generic overridable matrix implementation for GE implementatiom
*/

#include <Motion.hpp>

namespace Motion
{
    #define MATRIX_MINIMUM_WARNING_X                1024    
    #define MATRIX_MINIMUM_WARNING_Y                1024    

    /// @brief a matrix
    /// @tparam T the typename of the matrix
    /// @tparam X the X-size of the matrix
    /// @tparam Y the Y-size of the matrix
    template <typename T, int32_t X, int32_t Y>
    class Matrix
    {
    public:
        Matrix()
        {
            MOTION_ASSERT_FATAL(X <= 0 || Y <= 0, "Tried to create a matrix with a size of zero ?????");
            MOTION_ASSERT_WARNING(X >= MATRIX_MINIMUM_WARNING_X || Y >= MATRIX_MINIMUM_WARNING_Y, std::format("That matrix will use {} bytes of memory, are you sure you intended this?", MATRIX_MINIMUM_WARNING_X * MATRIX_MINIMUM_WARNING_Y));
            return; 
        }

        //
        // OPERATORS
        //

        T& operator[](std::size_t r, std::size_t c) 
        {
            return data[(r * Y) + c];
        }

        Matrix<T, X, Y> operator+(Matrix<T, X, Y>& other)
        {
            Matrix<T, X, Y> mat;
            
            for (std::size_t y = 0; y < Y; y++)
            {
                for (std::size_t x = 0; x < X; x++)
                    mat[x][y] = this[x][y] + other[x][y];
            }
        }

        Matrix<T, X, Y> operator-(Matrix<T, X, Y>& other)
        {
            Matrix<T, X, Y> mat;
            
            for (std::size_t y = 0; y < Y; y++)
            {
                for (std::size_t x = 0; x < X; x++)
                    mat[x][y] = this[x][y] - other[x][y];
            }
        }

        // operator * would be inefficient as we would have to do a copy
        template <int32_t P> // ugh
        Matrix<T, X, P> operator*=(Matrix<T, Y, P>& other)
        {
            // columns in first == rows in second
            // (m x n) * (n x p) = (m x p) 
            // x = columns, y = rows

            Matrix<T, X, P> final = {0};

            std::span<int, X> rowView(&data[Y * X], X); // get an std::span view of the thing

            // "parallelised approach" https://mariusbancila.ro/blog/2025/12/12/multiplying-matrices-in-cpp/
            // this will run many times for e.g. GE 20.8 floats so need so be fast
            std::for_each(
                std::execution::par,
                rowView.begin(), rowView.end(),
                [&](std::size_t i)
                {
                    for (int32_t j = 0; j < Y; ++j)
                    {
                        T sum = 0;
                        for (std::size_t k = 0; k < Y; ++k)
                            sum += this[i * Y + k] * other[k * Y + k];

                        final[i * Y + j] = sum;
                    }
                }
            );
        }

        //
        // OTHER OPS
        //

        // get identity matrix
        static Matrix<T, X, Y> Identity()
        {
            Matrix<T, X, Y> mat;

            // set up the mat
            std::size_t x = 0;
            
            for (std::size_t y = 0; y < Y; y++)
            {
                mat[x][y] = 1;
                x++;
            }

            return mat;
        }    

        /// @brief transpose a matrix
        /// @return the matrix, transposed
        Matrix<T, Y, X> Transpose()
        {
            Matrix<T, Y, X> mat; 

            for (std::size_t y = 0; y < Y; y++)
            {
                for (std::size_t x = 0; x < X; x++)
                    mat[x][y] = this[y][x];
            }

            return mat;
        }

    private: 
        std::array<T, (X * Y)> data{};
    };
}