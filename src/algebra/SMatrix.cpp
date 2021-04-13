#include "SMatrix.h"

// Explict specialization of BLAS matrix-matrix mult for SerialMatrix<complex<double>>
template <>
SerialMatrix<std::complex<double>> SerialMatrix<std::complex<double>>::prod(
    const SerialMatrix<std::complex<double>>& that, const char& trans1,
    const char& trans2) {
  if(cols() != that.rows()) {
    Error e("Cannot multiply matrices for which lhs.cols != rhs.rows.");
  }
  SerialMatrix<std::complex<double>> ret(rows(), that.cols());  // newly sized matrix
  // throw away variables
  std::complex<double> alpha(1.0, 0.0);
  std::complex<double> beta(0.0, 0.0);
  zgemm_(trans1, trans2, rows(), that.cols(), cols(), alpha, mat,
         rows(), that.mat, that.rows(), beta, ret.mat, rows());
  return ret;
}

// Explicit specializiation of BLAS matrix-matrix mult for SerialMatrix<double>
template <>
SerialMatrix<double> SerialMatrix<double>::prod(const SerialMatrix<double>& that,
                                    const char& trans1, const char& trans2) {
  if(cols() != that.rows()) {
    Error e("Cannot multiply matrices for which lhs.cols != rhs.rows.");
  }
  SerialMatrix<double> ret(rows(), that.cols());  // newly sized matrix
  // throw away variables
  double alpha = 1.0;
  double beta = 0.0;
  dgemm_(trans1, trans2, rows(), that.cols(), cols(), alpha, mat,
         rows(), that.mat, that.rows(), beta, ret.mat, rows());
  return ret;
}

// Diagonalize a complex double hermitian matrix
template <>
std::tuple<std::vector<double>, SerialMatrix<std::complex<double>>>
SerialMatrix<std::complex<double>>::diagonalize() {
  if (numRows_ != numCols_) {
    Error e("Can not diagonalize non-square matrix");
  }
  std::vector<double> eigvals(numRows_);
  SerialMatrix<std::complex<double>> eigvecs(numRows_, numCols_);

  // throw away variables
  SerialMatrix<std::complex<double>> temp;
  temp = *this;  // copy

  char jobz = 'V';
  char uplo = 'U';
  int lwork = std::max(1, 2 * numRows_ - 1) * 2;
  std::vector<std::complex<double>> work(lwork);
  int rworkSize = std::max(1, 3 * numRows_ - 2);
  std::vector<double> rwork(rworkSize);
  int info;

  zheev_(&jobz, &uplo, &numRows_, temp.mat, &numRows_, &eigvals[0], &work[0], &lwork,
         &rwork[0], &info);

  if (info != 0) {
    Error e("ZHEEV failed in SMatrix.", info);
  }
  return {eigvals, eigvecs};
}

// Diagonalize for real double symmetric matrix
template <>
std::tuple<std::vector<double>, SerialMatrix<double>> SerialMatrix<double>::diagonalize() {
  if (numRows_ != numCols_) {
    Error e("Can not diagonalize non-square matrix");
  }
  std::vector<double> eigvals(numRows_);
  SerialMatrix<double> eigvecs = *this;
  // throw away variables

  char jobz = 'V';
  char uplo = 'U';
  int lwork = std::max(1, 3 * numRows_ - 1) * 2;
  std::vector<double> work(lwork);
  int info;

  dsyev_(&jobz, &uplo, &numRows_, eigvecs.mat, &numRows_, &eigvals[0], &work[0],
         &lwork, &info);

  if (info != 0) {
    Error e("DSYEV failed in SMatrix.", info);
  }
  return {eigvals, eigvecs};
}

// Explicit specialization of norm for doubles
template <>
double SerialMatrix<double>::norm() {
  char norm = 'F';  // tells lapack to give us Frobenius norm
  int nr = numRows_;
  int nc = numCols_;
  std::vector<double> work(numRows_);
  return dlange_(&norm, &nr, &nc, this->mat, &nr, &work[0]);
}

// Explicit specialization of norm for complex doubles
template <>
double SerialMatrix<std::complex<double>>::norm() {
  char norm = 'F';  // tells lapack to give us Frobenius norm
  int nr = numRows_;
  int nc = numCols_;
  std::vector<double> work(numRows_);
  return zlange_(&norm, &nr, &nc, this->mat, &nr, &work[0]);
}
