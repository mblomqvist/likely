// Created 17-Apr-2012 by David Kirkby (University of California, Irvine) <dkirkby@uci.edu>

#ifndef LIKELY_COVARIANCE_MATRIX
#define LIKELY_COVARIANCE_MATRIX

#include "likely/types.h"

#include "boost/smart_ptr.hpp"

#include <vector>
#include <set>
#include <string>
#include <cstddef>
#include <iosfwd>

namespace likely {
    // Represents a covariance matrix.
	class CovarianceMatrix {
	public:
	    // Creates a new size-by-size covariance matrix with all elements initialized to zero.
	    // Throws a RuntimeError if size <= 0. Note that the matrix created by this constructor
	    // is not valid until sufficient elements have been set to make it positive definite.
		explicit CovarianceMatrix(int size);
		// Creates a new covariance matrix initialized with the specified elements, provided
		// as a column-wise packed vector:
		//
		// m00 m01 m02 ... 
		//     m11 m12 ...  ==> { m00, m01, m11, m02, m12, m22, ... }
		//         m22 ...
		//             ...
		//
		// The corresponding index calculation is m(i,j) = array[i+j*(j+1)/2] for i<=j. The
		// matrix size will be inferred from the input vector size using symmetricMatrixSize.
        explicit CovarianceMatrix(std::vector<double> packed);
		virtual ~CovarianceMatrix();

		// Assignment operator.
        CovarianceMatrix& operator=(CovarianceMatrix other);
        friend void swap(CovarianceMatrix& a, CovarianceMatrix& b);

		// Returns the fixed size of this covariance matrix.
        int getSize() const;
        // Returns the number of non-zero covariance matrix elements stored in this object.
        int getNElements() const;
        // Returns the (natural) log of the determinant of this covariance matrix.
        double getLogDeterminant() const;
        
        // Returns the specified (inverse) covariance matrix element or throws a RuntimeError.
        // (row,col) and (col,row) give the same result by construction. Be aware that
        // going back and forth between Covariance and InverseCovariance operations requires
        // potentially expensive matrix operations.
        double getCovariance(int row, int col) const;
        double getInverseCovariance(int row, int col) const;
        // Sets the specified (inverse) covariance matrix element or throws a RuntimeError.
        // Row and column indices should be in the range [0,size-1]. Setting any element with
        // row != col will also set the symmetric element in the matrix. Diagonal elements
        // (row == col) must be positive. Be aware that going back and forth between Covariance
        // and InverseCovariance operations requires potentially expensive matrix operations.
        void setCovariance(int row, int col, double value);
        void setInverseCovariance(int row, int col, double value);

        // Multiplies the specified vector by the (inverse) covariance or throws a RuntimeError.
        // The result is stored in the input vector, overwriting its original contents.
        void multiplyByCovariance(std::vector<double> &vector) const;
        void multiplyByInverseCovariance(std::vector<double> &vector) const;
        // Calculates the chi-square = delta.Cinv.delta for the specified residuals vector delta
        // or throws a RuntimeError.
        double chiSquare(std::vector<double> const &delta) const;

        // Multiplies all elements of the covariance matrix by the specified positive scale factor.
        void applyScaleFactor(double scaleFactor);
        // Replaces the original covariance matrix contents C with the triple matrix
        // product A.Cinv.A for the specified other covariance matrix A. For A,C both positive
        // definite, the result is a new (positive definite) covariance matrix.
        void replaceWithTripleProduct(CovarianceMatrix const &other);
        // Adds each element of the inverse of the specified CovarianceMatrix to our inverse
        // elements, using the specified weight (which must be positive in order to preserve
        // our positive-definiteness). If the other matrix is compressed, this method will
        // not uncompress it.
        void addInverse(CovarianceMatrix const &other, double weight = 1);

        // Fills the vector provided with a single random sampling of the Gausian probability
        // density implied by this object, or throws a RuntimeError. Returns the value of
        // delta.Cinv.delta/2 which is the negative log-likelihood of the generated sample.
        // Uses the random generator provided or else the default Random::instance().
        double sample(std::vector<double> &delta, RandomPtr random = RandomPtr()) const;
        // Generates the specified number of random residuals vectors by sampling the Gaussian
        // probability density implied by this object, or throws a RuntimeError. The generated
        // vectors are stored consecutively in the returned shared_array object, which will
        // have at least nsample*getSize() elements (the actual size might be bigger than this
        // for faster generation.) The elements k of the n-th generated vector are at
        // array[n*getSize()+k] with n = 0..(nsample-1) and k = 0..(getSize()-1). All memory
        // allocated by this method will be freed when the returned shared array's reference
        // count goes to zero. See above for random parameter usage. This method is optimized for
        // generating large numbers of residual vectors, and is slower than repeated use of
        // the single-sample method above for small values of nsample (on a macbookpro, the
        // crossover is around nsample = 32 and this method is ~4x faster for large nsample).
        boost::shared_array<double> sample(int nsample, RandomPtr random = RandomPtr()) const;
        
        // Prunes this covariance matrix by eliminating any rows and columns corresponding to
        // indices not specified in the keep set. Throws a RuntimeError if any indices are
        // out of range. Pruning is done in place and does not require any new memory allocation.
        void prune(std::set<int> const &keep);

        // Prints our covariance matrix elements to the specified output stream, using the
        // specified printf format for each element. If normalized is true, then print
        // sqrt of diagonal elements and normalize off-diagonal elements as correlation
        // coefficients rho(i,j) = cov(i,j)/sqrt(cov(i,i)*cov(j,j)). If an optional vector
        // of labels is provided, it will be used to label each row.
        void printToStream(std::ostream &os, bool normalized = false,
            std::string format = std::string("%+10.3lg"),
            std::vector<std::string> const &labels = std::vector<std::string>()) const;
        // Requests that this covariance matrix be compressed to reduce its memory usage,
        // if possible. Returns immediately if we are already compressed. Any compression
        // is lossless. The next call to any method except getSize(), compress(), or isCompressed().
        // will automatically trigger a decompression. However, a compressed matrix can be
        // added to another matrix (via addInverse) without being uncompressed. Return value
        // indicates if any compression was actually performed.
        bool compress() const;
        // Returns true if this covariance matrix is currently compressed.
        bool isCompressed() const;
        // Returns the memory usage of this object.
        std::size_t getMemoryUsage() const;
        // Returns a string describing this object's internal state in the form
        // 
        // [MICDZV] nnnnnnn
        //
        // where each letter indicates the memory allocation state of an internal
        // vector and nnnnnn is the total number of bytes used by this object, as reported
        // by getMemoryUsage(). The letter codes are: M = _cov, I = _icov, C = _cholesky,
        // D = _diag, Z = _offdiagIndex, V = _offdiagValue. A "-" indidcates that the vector
        // is not allocated. A "." below is a wildcard.
        //
        // [---...] : newly created object with no elements set
        // [M--...] : most recent change was to covariance matrix
        // [-I-...] : most recent change was to inverse covariance matrix
        // [MI-...] : synchronized covariance and inverse covariance both in memory
        // [--C...] : ** this should never happen **
        // [M-C...] : Cholesky decomposition and covariance in memory
        // [-IC...] : Cholesky decomposition and inverse covariance in memory
        // [MIC...] : Cholesky decomposition, covariance and inverse covariance in memory
        // [...D--] : Matrix is diagonal and compressed
        // [...DZV] : Matrix is non-diagonal and compressed
        std::string getMemoryState() const;
        
    private:
        // Undoes any compression. Returns immediately if we are already uncompressed.
        // There is usually no need to call this method explicitly, since it is called
        // automatically as needed by other methods.
        void _uncompress() const;
	    // Prepares to read elements of _cov or _icov. Returns false if nothing has
	    // been allocated yet, or else returns true. Always uncompresses.
        bool _readsCov() const;
        bool _readsICov() const;
        // Prepares to read the Cholesky decomposition of the covariance stored in _cholesky.
        void _readsCholesky() const;
        // Prepares to change at least one element of _cov or _icov.
        void _changesCov();
        void _changesICov();
        // Helper function used by getMemoryState()
        char _tag(char symbol, std::vector<double> const &vector) const;

        // TODO: is a cached value of _ncov = (_size*(_size+1))/2 really necessary?
        int _size, _ncov;
        // Track our compression state. This is not the same as !_diag.empty() since we
        // cache previous compression data until a change to _cov or _icov invalidates it.
        mutable bool _compressed;
        // _cholesky is the Cholesky decomposition of the covariance matrix (_cov, not _icov)
        mutable std::vector<double> _cov, _icov, _cholesky;
        // compression replaces _cov, _icov, _cholesky with the following
        // smaller vectors, that encode the inverse covariance matrix (_icov not _cov).
        mutable std::vector<double> _diag, _offdiagIndex, _offdiagValue;
	}; // CovarianceMatrix
	
    void swap(CovarianceMatrix& a, CovarianceMatrix& b);

    inline int CovarianceMatrix::getSize() const { return _size; }
    
    inline bool CovarianceMatrix::isCompressed() const { return _compressed; }

    // Returns the array offset index for the BLAS packed symmetric matrix format
    // described at http://www.netlib.org/lapack/lug/node123.html or throws a
    // RuntimeError for invalid row or col inputs.
    int symmetricMatrixIndex(int row, int col, int size);
    // Returns the size of a symmetric matrix in the BLAS packed format implied by
    // symmetricMatrixIndex, or throws a RuntimeError. The size is related to the
    // number nelem of packed matrix elements by size = (nelem*(nelem+1))/2.
    int symmetricMatrixSize(int nelem);
    // Performs a Cholesky decomposition in place of a symmetric positive definite matrix
    // or throws a RuntimeError if the matrix is not positive definite. The input matrix
    // is assumed to be in the BLAS packed format implied by packedMatrixIndex(row,col).
    // The matrix size will be calculated unless a positive value is provided.
    static void choleskyDecompose(std::vector<double> &matrix, int size = 0);
    // Inverts a symmetric positive definite matrix in place, or throws a RuntimeError.
    // The input matrix should already be Cholesky decomposed and in the BLAS packed format
    // implied by packedMatrixIndex(row,col), e.g. by first calling _choleskyDecompose(matrix).
    // The matrix size will be calculated unless a positive value is provided.
    static void invertCholesky(std::vector<double> &matrix, int size = 0);
    // Multiplies a symmetric matrix by a vector, or throws a RuntimeError. The input matrix
    // is assumed to be in the BLAS packed format implied by packedMatrixIndex(row,col).
    static void symmetricMatrixMultiply(std::vector<double> const &matrix,
        std::vector<double> const &vector, std::vector<double> &result);
        
    // Creates a diagonal covariance matrix with constant elements (first form) or specified
    // positive elements (second form).
    CovarianceMatrixPtr createDiagonalCovariance(int size, double diagonalValue = 1);
    CovarianceMatrixPtr createDiagonalCovariance(std::vector<double> diagonalValues);
    // Generates a random symmetric positive-definite matrix with the specified scale, which
    // fixes the determinant of the generated matrix to scale^size, which is the determinant of
    // scale*[identity matrix] and means that the generated covariances are directly proportional
    // to scale. Uses the random generator provided or else the default Random::instance().
    CovarianceMatrixPtr generateRandomCovariance(int size, double scale = 1,
        RandomPtr random = RandomPtr());

} // likely

#endif // LIKELY_COVARIANCE_MATRIX