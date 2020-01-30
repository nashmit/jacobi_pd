///   @file  jacobi.hpp
///   @brief Calculate the eigenvalues and eigevectors of a symmetric matrix
///          using the Jacobi eigenvalue algorithm.
#ifndef _JACOBI_HPP
#define _JACOBI_HPP

#include <cmath>
#include <cassert>
#include "matrix_alloc.hpp"

namespace jacobi_public_domain {

using namespace matrix_alloc;

/// @class Jacobi
/// @brief Calculate the eigenvalues and eigevectors of a symmetric matrix
///        using the Jacobi eigenvalue algorithm.
///        The algorithm implemented here follows the strategy explained here:
///        https://en.wikipedia.org/wiki/Jacobi_eigenvalue_algorithm
///        https://web.stanford.edu/class/cme335/lecture7.pdf
/// @note  The "Vector" and "Matrix" type arguments can be any 
///        C or C++ object that support indexing, including pointers or vectors.

template<typename Scalar,typename Vector,typename Matrix,typename ConstMatrix>

class Jacobi
{
  int n;            //!< the size of the matrix
  Scalar **M;       //!< local copy of the matrix being analyzed
  // Precomputed cosine, sin, and tangent of the most recent rotation angle:
  Scalar c;         //!< = cos(θ)
  Scalar s;         //!< = sin(θ)
  Scalar t;         //!< = tan(θ),  (note |t|<=1)
  int *max_idx_row; //!< for row i, the index j of the maximum element where j>i

public:

  /// @brief  Specify the size of the matrices you want to diagonalize later.
  /// @param n  the size (ie. number of rows) of the square matrix
  Jacobi(int n) {
    Init();
    SetSize(n);
  }

  // @typedef choose the criteria for sorting eigenvalues and eigenvectors
  typedef enum eSortCriteria {
    DO_NOT_SORT,
    SORT_DECREASING_EVALS,
    SORT_INCREASING_EVALS,
    SORT_DECREASING_ABS_EVALS,
    SORT_INCREASING_ABS_EVALS
  } SortCriteria;

  /// @brief Calculate the eigenvalues and eigevectors of a symmetric matrix
  ///        using the Jacobi eigenvalue algorithm:
  ///        https://en.wikipedia.org/wiki/Jacobi_eigenvalue_algorithm
  /// @returns The number_of_sweeps (= number_of_iterations / (n*(n-1)/2)).
  ///          If this equals max_num_sweeps, the algorithm failed to converge.
  /// @note  To reduce the computation time further, set calc_evects=false.
  int
  Diagonalize(ConstMatrix mat, //!< the matrix you wish to diagonalize (size n)
              Vector eval,   //!< store the eigenvalues here
              Matrix evec,   //!< store the eigenvectors here (in rows)
              SortCriteria sort_criteria=SORT_DECREASING_EVALS,//!<sort results?
              bool calc_evects=true,     //!< calculate the eigenvectors?
              int max_num_sweeps = 50);  //!< limit the number of iterations

private:

  /// @brief Calculate the components of a rotation matrix which performs a
  ///        rotation in the i,j plane by an angle (θ) that (when multiplied on
  ///        both sides) will zero the ij'th element of M, so that afterwards
  ///        M[i][j] = 0.  The results will be stored in c, s, and t
  ///        (which store cos(θ), sin(θ), and tan(θ), respectively).
  void CalcRot(Matrix M,   //!< matrix
               int i,      //!< row index
               int j);     //!< column index

  /// @brief Apply the (previously calculated) rotation matrix to matrix M
  ///        by multiplying it on both sides (a "similarity transform").
  ///        (To save time, only update the elements in the upper-right
  ///         triangular region of the matrix.  It is assumed that i < j.)
  void ApplyRot(Matrix M,  //!< matrix
                int i,     //!< row index
                int j);    //!< column index

  /// @brief Multiply matrix E on the left by the (previously calculated)
  ///        rotation matrix.
  void ApplyRotLeft(Matrix E,  //!< matrix
                    int i,     //!< row index
                    int j);    //!< column index

  ///@brief Find the off-diagonal index in row i whose absolute value is largest
  int MaxEntryRow(Matrix M, int i) const;

  /// @brief Find the indices (i_max, j_max) marking the location of the
  ///        entry in the matrix with the largest absolute value.  This
  ///        uses the max_idx_row[] array to find the answer in O(n) time.
  /// @returns This function does not return avalue.  However after it is
  ///          invoked, the location of the largest matrix element will be
  ///          stored in the i_max and j_max arguments.
  void MaxEntry(Matrix M, int& i_max, int& j_max) const;

  // @brief Sort the rows in M according to the numbers in v (also sorted)
  void SortRows(Vector v, //!< vector containing the keys used for sorting
                Matrix M, //!< matrix whose rows will be sorted according to v
                int n,    //!< size of the vector and matrix
                SortCriteria s=SORT_DECREASING_EVALS //!< sort decreasing order?
                ) const;

  // memory management:
  void SetSize(int n);
  void Alloc(int n);
  void Init();
  void Dealloc();

  // memory management: copy constructor, swap, and assignment operator
  Jacobi(const Jacobi<Scalar, Vector, Matrix, ConstMatrix>& source);
  void swap(Jacobi<Scalar, Vector, Matrix, ConstMatrix> &other);
  Jacobi<Scalar, Vector, Matrix, ConstMatrix>& operator = (Jacobi<Scalar, Vector, Matrix, ConstMatrix> source);

}; // class Jacobi





// -------------- IMPLEMENTATION --------------



template<typename Scalar,typename Vector,typename Matrix,typename ConstMatrix>
int Jacobi<Scalar, Vector, Matrix, ConstMatrix>::
Diagonalize(ConstMatrix mat,    // the matrix you wish to diagonalize (size n)
            Vector eval,        // store the eigenvalues here
            Matrix evec,        // store the eigenvectors here (in rows)
            SortCriteria sort_criteria, // sort results?
            bool calc_evec,     // calculate the eigenvectors?
            int max_num_sweeps) // limit the number of iterations ("sweeps")
{
  // -- Initialization --
  for (int i = 0; i < n; i++)
    for (int j = i; j < n; j++)          //copy mat[][] into M[][]
      M[i][j] = mat[i][j];               //(M[][] is a local copy we can modify)

  if (calc_evec)
    for (int i = 0; i < n; i++)
      for (int j = 0; j < n; j++)        //Initialize the evec[][] matrix.
        evec[i][j] = (i==j) ? 1.0 : 0.0; //Set evec equal to the identity matrix

  for (int i = 0; i < n-1; i++)          //Initialize the "max_idx_row[]" array 
    max_idx_row[i] = MaxEntryRow(M, i);  //(which is needed by MaxEntry())

  // -- Iteration --
  int n_iters;
  int max_num_iters = max_num_sweeps*n*(n-1)/2; //"sweep" = n*(n-1)/2 iters
  for (n_iters=0; n_iters < max_num_iters; n_iters++) {
    int i,j;
    MaxEntry(M, i, j); // Find the maximum entry in the matrix. Store in i,j

    if (M[i][j] == 0.0)
      break;

    // If M[i][j] is small compared to M[i][i] and M[j][j], set it to 0.
    if ((M[i][i] + M[i][j] == M[i][i]) && (M[j][j] + M[i][j] == M[j][j])) {
      M[i][j] = 0.0;
      max_idx_row[i] = MaxEntryRow(M,i); //must also update max_idx_row[i]
      continue;
    }

    // Otherwise, apply a rotation to make M[i][j] = 0
    CalcRot(M, i, j);  // Calculate the parameters of the rotation matrix.
    ApplyRot(M, i, j); // Apply this rotation to the M matrix.
    if (calc_evec)     // Optional: If the caller wants the eigenvectors, then
      ApplyRotLeft(evec,i,j); // apply the rotation to the eigenvector matrix

  } //for (int n_iters=0; n_iters < max_num_iters; n_iters++)

  // -- Post-processing --
  for (int i = 0; i < n; i++)
    eval[i] = M[i][i];

  // Sort eigenvalues and eigenvectors?
  if (sort_criteria != DO_NOT_SORT)
    SortRows(eval, evec, n, sort_criteria);

  return n_iters / (n*(n-1)/2); //returns the number of "sweeps" (converged?)
}


/// @brief Calculate the components of a rotation matrix which performs a
///        rotation in the i,j plane by an angle (θ) that (when multiplied on
///        both sides) will zero the ij'th element of M, so that afterwards
///        M[i][j] = 0.  The results will be stored in c, s, and t
///        (which store cos(θ), sin(θ), and tan(θ), respectively).

template<typename Scalar,typename Vector,typename Matrix,typename ConstMatrix>
void Jacobi<Scalar, Vector, Matrix, ConstMatrix>::
CalcRot(Matrix M,    // matrix
        int i,       // row index
        int j)       // column index
{
  t = 1.0; // = tan(θ)
  Scalar M_jj_ii = (M[j][j] - M[i][i]);
  if (M_jj_ii != 0.0) {
    // kappa = (M[j][j] - M[i][i]) / (2*M[i][j])
    Scalar kappa = M_jj_ii;
    t = 0.0;
    Scalar M_ij = M[i][j];
    if (M_ij != 0.0) {
      kappa /= (2.0*M_ij);
      // t satisfies: t^2 + 2*t*kappa - 1 = 0
      // (choose the root which has the smaller absolute value)
      t = 1.0 / (std::sqrt(1 + kappa*kappa) + std::abs(kappa));
      if (kappa < 0.0)
        t = -t;
    }
  }
  assert(std::abs(t) <= 1.0);
  c = 1.0 / std::sqrt(1 + t*t);
  s = c*t;
}


/// brief   Perform a similarity transform by multiplying matrix M on both
///         sides by a rotation matrix (transposing one of them).
///         This rotation matrix performs a rotation in the i,j plane by
///         angle θ.  Also updates the max_idx_row[] array.  Assumes i < j.
/// details This function assumes that i<j and that cos(θ), sin(θ), and tan(θ)
///         have already been computed (by invoking CalcRot()).
///         To save time, since the matrix is symmetric, the elements
///         below the diagonal (ie. M[u][v] where u>v) are not computed.
/// @code
///   M' = R^T * M * R
/// where R the rotation in the i,j plane and ^T denotes the transpose.
///                 i         j
///       _                             _
///      |  1                            | 
///      |    .                          |
///      |      .                        |
///      |        1                      |
///      |          c   ...   s          |
///      |          .  .      .          |
/// R  = |          .    1    .          |
///      |          .      .  .          |
///      |          -s  ...   c          |
///      |                      1        |
///      |                        .      |
///      |                          .    |
///      |_                           1 _|
/// @endcode
///
/// Let M' denote the matrix M after multiplication by R^T and R.
/// The components of M' are:
///   M'_uv =  Σ_w  Σ_z   R_wu * M_wz * R_zv
///
/// Note that a the rotation at location i,j will modify all of the matrix
/// elements containing at least one index which is either i or j
/// such as: M[w][i], M[i][w], M[w][j], M[j][w].
/// Check and see whether these modified matrix elements exceed the 
/// corresponding values in max_idx_row[] array for that row.
/// If so, then update max_idx_row for that row.
/// This is somewhat complicated by the fact that we must only consider
/// matrix elements in the upper-right triangle strictly above the diagonal.
/// (ie. matrix elements whose second index is > the first index).
/// The modified elements we must consider are marked with an "X" below:
///
/// @code
///                 i         j
///       _                             _
///      |  .       X         X          | 
///      |    .     X         X          |
///      |      .   X         X          |
///      |        . X         X          |
///      |          X X X X X 0 X X X X  |  i
///      |            .       X          |
///      |              .     X          |
/// M  = |                .   X          |
///      |                  . X          |
///      |                    X X X X X  |  j
///      |                      .        |
///      |                        .      |
///      |                          .    |
///      |_                           . _|
/// @endcode

template<typename Scalar,typename Vector,typename Matrix,typename ConstMatrix>
void Jacobi<Scalar, Vector, Matrix, ConstMatrix>::
ApplyRot(Matrix M,  // matrix
         int i,     // row index
         int j)     // column index
{
  // Recall that:
  // c = cos(θ)
  // s = sin(θ)
  // t = tan(θ) (which should be <= 1.0)

  // Compute the diagonal elements of M which have changed:
  M[i][i] -= t * M[i][j];
  M[j][j] += t * M[i][j];
  // Note: This is algebraically equivalent to:
  // M[i][i] = c*c*M[i][i] + s*s*M[j][j] - 2*s*c*M[i][j]
  // M[j][j] = s*s*M[i][i] + c*c*M[j][j] + 2*s*c*M[i][j]

  //Update the off-diagonal elements of M which will change (above the diagonal)

  assert(i < j);

  M[i][j] = 0.0;

  //compute M[w][i] and M[i][w] for all w!=i,considering above-diagonal elements
  for (int w=0; w < i; w++) {        // 0 <= w <  i  <  j < n
    M[i][w] = M[w][i]; //backup the previous value. store below diagonal (i>w)
    M[w][i] = c*M[w][i] - s*M[w][j]; //M[w][i], M[w][j] from previous iteration
    if (i == max_idx_row[w]) max_idx_row[w] = MaxEntryRow(M, w);
    else if (std::abs(M[w][i])>std::abs(M[w][max_idx_row[w]])) max_idx_row[w]=i;
    //assert(max_idx_row[w] == MaxEntryRow(M, w));
  }

  for (int w=i+1; w < j; w++) {      // 0 <= i <  w  <  j < n
    M[w][i] = M[i][w]; //backup the previous value. store below diagonal (w>i)
    M[i][w] = c*M[i][w] - s*M[w][j]; //M[i][w], M[w][j] from previous iteration
  }
  for (int w=j+1; w < n; w++) {      // 0 <= i < j+1 <= w < n
    M[w][i] = M[i][w]; //backup the previous value. store below diagonal (w>i)
    M[i][w] = c*M[i][w] - s*M[j][w]; //M[i][w], M[j][w] from previous iteration
  }

  // now that we're done modifying row i, we can update max_idx_row[i]
  max_idx_row[i] = MaxEntryRow(M, i);

  //compute M[w][j] and M[w][j] for all w!=j,considering above-diagonal elements
  for (int w=0; w < i; w++) {        // 0 <=  w  <  i <  j < n
    M[w][j] = s*M[i][w] + c*M[w][j]; //M[i][w], M[w][j] from previous iteration
    if (j == max_idx_row[w]) max_idx_row[w] = MaxEntryRow(M, w);
    else if (std::abs(M[w][j])>std::abs(M[w][max_idx_row[w]])) max_idx_row[w]=j;
    //assert(max_idx_row[w] == MaxEntryRow(M, w));
  }
  for (int w=i+1; w < j; w++) {      // 0 <= i+1 <= w <  j < n
    M[w][j] = s*M[w][i] + c*M[w][j]; //M[w][i], M[w][j] from previous iteration
    if (j == max_idx_row[w]) max_idx_row[w] = MaxEntryRow(M, w);
    else if (std::abs(M[w][j])>std::abs(M[w][max_idx_row[w]])) max_idx_row[w]=j;
    //assert(max_idx_row[w] == MaxEntryRow(M, w));
  }
  for (int w=j+1; w < n; w++) {      // 0 <=  i  <  j <  w < n
    M[j][w] = s*M[w][i] + c*M[j][w]; //M[w][i], M[j][w] from previous iteration
  }
  // now that we're done modifying row j, we can update max_idx_row[j]
  max_idx_row[j] = MaxEntryRow(M, j);

} //Jacobi::ApplyRot()




/// brief Multiply matrix M on the LEFT side by a transposed rotation matrix R^T
///       This matrix performs a rotation in the i,j plane by angle θ  (where
///       the arguments "s" and "c" refer to cos(θ) and sin(θ), respectively).
/// @code
///   E'_uv = Σ_w  R_wu * E_wv
/// @endcode

template<typename Scalar,typename Vector,typename Matrix,typename ConstMatrix>
void Jacobi<Scalar, Vector, Matrix, ConstMatrix>::
ApplyRotLeft(Matrix E,  // matrix
             int i,     // row index
             int j)     // column index
{
  // Recall that c = cos(θ) and s = sin(θ)
  for (int v = 0; v < n; v++) {
    Scalar Eiv = E[i][v]; //backup E[i][v]
    E[i][v] = c*E[i][v] - s*E[j][v];
    E[j][v] = s*Eiv     + c*E[j][v];
  }
}



template<typename Scalar,typename Vector,typename Matrix,typename ConstMatrix>
int Jacobi<Scalar, Vector, Matrix, ConstMatrix>::
MaxEntryRow(Matrix M, int i) const {
  int j_max = i+1;
  for(int j = i+2; j < n; j++)
    if (std::abs(M[i][j]) > std::abs(M[i][j_max]))
      j_max = j;
  return j_max;
}



template<typename Scalar,typename Vector,typename Matrix,typename ConstMatrix>
void Jacobi<Scalar, Vector, Matrix, ConstMatrix>::
MaxEntry(Matrix M, int& i_max, int& j_max) const {
  // find the maximum entry in the matrix M in O(n) time
  i_max = 0; // (start with an arbitrary
  j_max = 1; //  off-diagonal element: M[0][1])
  Scalar max_entry = std::abs(M[i_max][j_max]);
  int nm1 = n-1;
  for (int i=0; i < nm1; i++) {
    int j = max_idx_row[i];
    if (std::abs(M[i][j]) > max_entry) {
      max_entry = std::abs(M[i][j]);
      i_max = i;
      j_max = j;
    }
  }
  //#ifndef NDEBUG
  //// -- remove the next 4 lines before publishing --
  //// make sure that the maximum element really is stored at i_max, j_max
  //for (int i = 0; i < nm1; i++)
  //  for (int j = i+1; j < n; j++)
  //    assert(std::abs(M[i][j]) <= max_entry);
  //// --
  //#endif
}



//Sort the rows of a matrix "evec" by the numbers contained in "eval"
template<typename Scalar,typename Vector,typename Matrix,typename ConstMatrix>
void Jacobi<Scalar, Vector, Matrix, ConstMatrix>::
SortRows(Vector eval, Matrix evec, int n, SortCriteria sort_criteria) const
{
  for (int i = 0; i < n-1; i++) {
    int i_max = i;
    for (int j = i+1; j < n; j++) {
      // find the "maximum" element in the array starting at position i+1
      switch (sort_criteria) {
      case SORT_DECREASING_EVALS:
        if (eval[j] > eval[i_max])
          i_max = j;
        break;
      case SORT_INCREASING_EVALS:
        if (eval[j] < eval[i_max])
          i_max = j;
        break;
      case SORT_DECREASING_ABS_EVALS:
        if (std::abs(eval[j]) > std::abs(eval[i_max]))
          i_max = j;
        break;
      case SORT_INCREASING_ABS_EVALS:
        if (std::abs(eval[j]) < std::abs(eval[i_max]))
          i_max = j;
        break;
      default:
        assert(false);
        break;
      }
    }
    std::swap(eval[i], eval[i_max]); // sort "eval"
    for (int k = 0; k < n; k++)
      std::swap(evec[i][k], evec[i_max][k]); // sort "evec"
  }
}



template<typename Scalar,typename Vector,typename Matrix,typename ConstMatrix>
void Jacobi<Scalar, Vector, Matrix, ConstMatrix>::
Init() {
  n = 0;
  M = nullptr;
  max_idx_row = nullptr;
}

template<typename Scalar,typename Vector,typename Matrix,typename ConstMatrix>
void Jacobi<Scalar, Vector, Matrix, ConstMatrix>::
SetSize(int n) {
  Dealloc();
  Alloc(n);
}

template<typename Scalar,typename Vector,typename Matrix,typename ConstMatrix>
void Jacobi<Scalar, Vector, Matrix, ConstMatrix>::
Alloc(int n) {
  this->n = n;
  max_idx_row = new int[n];
  Alloc2D(n, n, &M);
}

template<typename Scalar,typename Vector,typename Matrix,typename ConstMatrix>
void Jacobi<Scalar, Vector, Matrix, ConstMatrix>::
Dealloc() {
  if (max_idx_row)
    delete [] max_idx_row;
  Dealloc2D(&M);
  Init();
}

template<typename Scalar,typename Vector,typename Matrix,typename ConstMatrix>
Jacobi<Scalar, Vector, Matrix, ConstMatrix>::
Jacobi(const Jacobi<Scalar, Vector, Matrix, ConstMatrix>& source)
{
  Init();
  Alloc(source.n);
  assert(n == source.n);
  std::copy(source.max_idx_row,
            source.max_idx_row + n,
            max_idx_row);
  for (int i = 0; i < n; i++)
    std::copy(source.M[i],
              source.M[i] + n,
              M[i]);
}

template<typename Scalar,typename Vector,typename Matrix,typename ConstMatrix>
void Jacobi<Scalar, Vector, Matrix, ConstMatrix>::
swap(Jacobi<Scalar, Vector, Matrix, ConstMatrix> &other) {
  std::swap(max_idx_row, other.max_idx_row);
  for (int i = 0; i < n; i++)
    std::swap(M[i], other.M[i]);
  std::swap(n, other.n);
}

template<typename Scalar,typename Vector,typename Matrix,typename ConstMatrix>
Jacobi<Scalar, Vector, Matrix, ConstMatrix>&
Jacobi<Scalar, Vector, Matrix, ConstMatrix>::
operator = (Jacobi<Scalar, Vector, Matrix, ConstMatrix> source) {
  this->swap(source);
  return *this;
}



} // namespace jacobi


#endif //#ifndef _JACOBI_HPP

