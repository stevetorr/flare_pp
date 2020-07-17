#ifndef SPARSE_GP_DTC_H
#define SPARSE_GP_DTC_H

#include "sparse_gp.h"
#include <Eigen/Dense>

class SparseGP_DTC : public SparseGP {
public:
  // TODO: Modify "add" methods to keep track of each kernel contribution.
  std::vector<Eigen::MatrixXd> Kuf_env_kernels, Kuf_struc_kernels, Kuu_kernels;
  Eigen::VectorXd noise_vector, y;
  Eigen::MatrixXd Sigma, Kuu_inverse, Kuf;
  int max_labels = 0;

  // Likelihood attributes.
  double log_marginal_likelihood, data_fit, complexity_penalty, trace_term,
    constant_term;
  Eigen::VectorXd likelihood_gradient;

  SparseGP_DTC();
  SparseGP_DTC(std::vector<Kernel *> kernels, double sigma_e, double sigma_f,
               double sigma_s);

  // Methods for augmenting the training set.
  void add_sparse_environments(const std::vector<LocalEnvironment> &envs);
  void add_training_structure(const StructureDescriptor &training_structure);

  // Not implemented (yet):
  void add_sparse_environment(const LocalEnvironment &env);
  void add_training_environment(const LocalEnvironment &training_environment);
  void add_training_environments(const std::vector<LocalEnvironment> &envs);

  // Update matrices and vectors needed for mean and variance prediction:
  // Sigma, Kuu_inverse, and alpha.
  void update_matrices();

  // Compute the DTC mean and variance.
  void predict_DTC(StructureDescriptor test_structure,
      Eigen::VectorXd & mean_vector, Eigen::VectorXd & variance_vector,
      std::vector<Eigen::VectorXd> & mean_contributions);

  // Calculate the log marginal likelihood of the current hyperparameters and its gradient.
  // TODO: Find a way to optimize the hyperparameters.
  void compute_DTC_likelihood();
  void compute_VFE_likelihood();

  // Change the model hyperparameters and covariance matrices.
  // With the dot product kernel, should never have to recompute kernels;
  // just rescale them.
  void set_hyperparameters(Eigen::VectorXd hyperparameters);
};

#endif
