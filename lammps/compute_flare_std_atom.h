// Yu Xie 
// Compute uncertainty per atom
// Based on pair_flare.h and compute_uncertainty_atom.h

#ifdef COMPUTE_CLASS

ComputeStyle(flare/std/atom, ComputeFlareStdAtom)

#else

#ifndef LMP_COMPUTE_FLARE_STD_ATOM_H
#define LMP_COMPUTE_FLARE_STD_ATOM_H

#include "compute.h"
#include <Eigen/Dense>
#include <cstdio>
#include <vector>

namespace LAMMPS_NS {

class ComputeFlareStdAtom : public Compute {
public:
  ComputeFlareStdAtom(class LAMMPS *, int, char **);
  ~ComputeFlareStdAtom();
  void compute_peratom();

  int pack_reverse_comm(int, int, double *);
  void unpack_reverse_comm(int, int *, double *);
  double memory_usage();
  //double init_one(int, int);
  void init();
  void init_list(int, class NeighList *);

protected:
  double *stds;
  double **desc_derv;
  class NeighList *list;

  int n_species, n_max, l_max, n_descriptors, beta_size;

  std::function<void(std::vector<double> &, std::vector<double> &, double, int,
                     std::vector<double>)>
      basis_function;
  std::function<void(std::vector<double> &, double, double,
                     std::vector<double>)>
      cutoff_function;

  std::vector<double> radial_hyps, cutoff_hyps;

  int nmax; // number of atoms
  double cutoff;
  double *beta;
  Eigen::MatrixXd beta_matrix;
  std::vector<Eigen::MatrixXd> beta_matrices;

  virtual void allocate();
  virtual void read_file(char *);
  void grab(FILE *, int, double *);

  virtual void coeff(int, char **);

  // below are defined in pair.h
  void settings(int, char **);
  int allocated=0;
  int **setflag;                 // 0/1 = whether each i,j has been set
  double **cutsq;                // cutoff sq for each atom pair
    
};

} // namespace LAMMPS_NS

#endif
#endif
