#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <Eigen/Dense>
#include "pair_flare.h"
#include "atom.h"
#include "force.h"
#include "comm.h"
#include "neighbor.h"
#include "neigh_list.h"
#include "neigh_request.h"
#include "memory.h"
#include "error.h"

// flare++ modules
#include "y_grad.h"
#include "cutoffs.h"
#include "radial.h"
#include "lammps_descriptor.h"

using namespace LAMMPS_NS;

#define MAXLINE 1024

/* ---------------------------------------------------------------------- */

PairFLARE::PairFLARE(LAMMPS *lmp) : Pair(lmp)
{
  restartinfo = 0;
  manybody_flag = 1;

  beta = NULL;
}

/* ----------------------------------------------------------------------
   check if allocated, since class can be destructed when incomplete
------------------------------------------------------------------------- */

PairFLARE::~PairFLARE()
{
  if (copymode) return;

  memory->destroy(beta);

  if (allocated) {
    memory->destroy(setflag);
    memory->destroy(cutsq);
  }
}

/* ---------------------------------------------------------------------- */

void PairFLARE::compute(int eflag, int vflag)
{
  int i, j, ii, jj, inum, jnum, itype, jtype, n_inner;
  double evdwl, delx, dely, delz, xtmp, ytmp, ztmp;
  double *coeff;
  int *ilist, *jlist, *numneigh, **firstneigh;

  evdwl = 0.0;
  ev_init(eflag,vflag);

  double **x = atom->x;
  double **f = atom->f;
  int *type = atom->type;
  int nlocal = atom->nlocal;
  int nall = nlocal + atom->nghost;
  int newton_pair = force->newton_pair;

  inum = list->inum;
  ilist = list->ilist;
  numneigh = list->numneigh;
  firstneigh = list->firstneigh;

  int beta_init, beta_counter;
  double B2_norm_squared, B2_val_1, B2_val_2;
  double fij[3];
  Eigen::VectorXd single_bond_vals, B2_vals, B2_env_dot;
  Eigen::MatrixXd single_bond_env_dervs, B2_env_dervs;

  for (ii = 0; ii < inum; ii++) {
    i = list->ilist[ii];
    jnum = numneigh[i];
    xtmp = x[i][0];
    ytmp = x[i][1];
    ztmp = x[i][2];
    jlist = firstneigh[i];

    // Count the atoms inside the cutoff.

    // TODO: Shorten loops by only considering neighbors within the cutoff.
    // Compute covariant descriptors.
    single_bond(x, type, jnum, i, xtmp, ytmp, ztmp, jlist,
        basis_function, cutoff_function, cutoff, n_species, n_max, l_max,
        radial_hyps, cutoff_hyps, single_bond_vals, single_bond_env_dervs);

    // Compute invariant descriptors.
    B2_descriptor(B2_vals, B2_env_dervs, B2_norm_squared, B2_env_dot,
        single_bond_vals, single_bond_env_dervs, n_species, n_max, l_max);

    // Compute local energy.
    beta_init = (type[ilist[ii]] - 1) * beta_size;
    beta_counter = beta_init;
    evdwl = 0.0;
    for (int j = 0; j < n_descriptors; j++) {
        B2_val_1 = B2_vals(j);
        for (int k = j; k < n_descriptors; k++) {
            B2_val_2 = B2_vals(k);
            evdwl += B2_val_1 * B2_val_2 * beta[beta_counter];
            beta_counter ++;
        }
    }
    evdwl /= B2_norm_squared;

    // Compute partial forces and stresses.
    for (int jj = 0; jj < jnum; jj++){

        // Zero the partial force vector.
        for (int l = 0; l < 3; l++) fij[l] = 0.0;

        beta_counter = beta_init;
        for (int m = 0; m < n_descriptors; m++){
            for (int n = m; n < n_descriptors; n++){
                for (int l = 0; l < 3; l++){
                    fij[l] +=
                        (-B2_env_dervs(3 * jj + l, m) * B2_vals(n)
                         -B2_vals(m) * B2_env_dervs(3 * jj + l, n)) *
                         beta[beta_counter];
                }
                beta_counter ++;
            }
        }

        // Add normalization contribution.
        for (int l = 0; l < 3; l++){
            fij[l] += 2 * evdwl * B2_env_dot(jj * 3 + l);
            fij[l] /= B2_norm_squared;
        }

        // Store partial forces.
        j = jlist[jj];
        f[i][0] -= fij[0];
        f[i][1] -= fij[1];
        f[i][2] -= fij[2];
        f[j][0] += fij[0];
        f[j][1] += fij[1];
        f[j][2] += fij[2];

        if (vflag){
            delx = xtmp - x[j][0];
            dely = ytmp - x[j][1];
            delz = ztmp - x[j][2];

            ev_tally_xyz(i, j, nlocal, newton_pair, 0.0, 0.0, fij[0],fij[1],
                fij[2], delx, dely, delz);
        }
    }

    // Compute local energy.
    if (eflag) ev_tally_full(i, 2.0 * evdwl, 0.0, 0.0, 0.0, 0.0, 0.0);
  }

  if (vflag_fdotr) virial_fdotr_compute();
}

/* ----------------------------------------------------------------------
   allocate all arrays
------------------------------------------------------------------------- */

void PairFLARE::allocate()
{
  allocated = 1;
  int n = atom->ntypes;

  memory->create(setflag,n+1,n+1,"pair:setflag");

  // Set the diagonal of setflag to 1 (otherwise pair.cpp will throw an error)
  for (int i = 1; i <= n; i++)
      setflag[i][i] = 1;

  // Create cutsq array (used in pair.cpp)
  memory->create(cutsq,n+1,n+1,"pair:cutsq");
}

/* ----------------------------------------------------------------------
   global settings
------------------------------------------------------------------------- */

void PairFLARE::settings(int narg, char **/*arg*/)
{
  // "flare" should be the only word after "pair_style" in the input file.
  if (narg > 0) error->all(FLERR,"Illegal pair_style command");
}

/* ----------------------------------------------------------------------
   set coeffs for one or more type pairs
   read DYNAMO funcfl file
------------------------------------------------------------------------- */

void PairFLARE::coeff(int narg, char **arg)
{
  if (!allocated) allocate();

  // Should be exactly 3 arguments following "pair_coeff" in the input file.
  if (narg != 3) error->all(FLERR,"Incorrect args for pair coefficients");

  // Ensure I,J args are "* *".
  if (strcmp(arg[0],"*") != 0 || strcmp(arg[1],"*") != 0)
    error->all(FLERR,"Incorrect args for pair coefficients");

  read_file(arg[2]);
}

/* ----------------------------------------------------------------------
   init specific to this pair style
------------------------------------------------------------------------- */

void PairFLARE::init_style()
{
  // Require newton on.
  if (force->newton_pair == 0)
    error->all(FLERR,"Pair style requires newton pair on");

  // Request a full neighbor list.
  int irequest = neighbor->request(this,instance_me);
  neighbor->requests[irequest]->half = 0;
  neighbor->requests[irequest]->full = 1;
}

/* ----------------------------------------------------------------------
   init for one type pair i,j and corresponding j,i
------------------------------------------------------------------------- */

double PairFLARE::init_one(int i, int j)
{
  // init_one is called for each i, j pair in pair.cpp after calling init_style.

  return cutoff;
}

/* ----------------------------------------------------------------------
   read potential values from a DYNAMO single element funcfl file
------------------------------------------------------------------------- */

void PairFLARE::read_file(char *filename)
{
    int me = comm->me;
    char line[MAXLINE], radial_string[MAXLINE], cutoff_string[MAXLINE];
    int radial_string_length, cutoff_string_length;
    FILE *fptr;

    // Check that the potential file can be opened.
    if (me == 0) {
        fptr = force->open_potential(filename);
        if (fptr == NULL) {
        char str[128];
        snprintf(str,128,"Cannot open EAM potential file %s",filename);
        error->one(FLERR,str);
        }
    }

    int tmp,nwords;
    if (me == 0) {
        fgets(line, MAXLINE, fptr);
        fgets(line, MAXLINE, fptr);
        sscanf(line, "%s", radial_string);  // Radial basis set
        radial_string_length = strlen(radial_string);
        fgets(line, MAXLINE, fptr);
        sscanf(line, "%i %i %i %i", &n_species, &n_max, &l_max, &beta_size);
        fgets(line, MAXLINE, fptr);
        sscanf(line, "%s", cutoff_string);  // Cutoff function
        cutoff_string_length = strlen(cutoff_string);
        fgets(line, MAXLINE, fptr);
        sscanf(line, "%lg", &cutoff);  // Cutoff
    }

    MPI_Bcast(&n_species, 1, MPI_INT, 0, world);
    MPI_Bcast(&n_max, 1, MPI_INT, 0, world);
    MPI_Bcast(&l_max, 1, MPI_INT, 0, world);
    MPI_Bcast(&beta_size, 1, MPI_INT, 0, world);
    MPI_Bcast(&cutoff, 1, MPI_DOUBLE, 0, world);
    MPI_Bcast(&radial_string_length, 1, MPI_INT, 0, world);
    MPI_Bcast(&cutoff_string_length, 1, MPI_INT, 0, world);
    MPI_Bcast(radial_string, radial_string_length + 1, MPI_CHAR, 0, world);
    MPI_Bcast(cutoff_string, cutoff_string_length + 1, MPI_CHAR, 0, world);

    // Set number of descriptors.
    int n_radial = n_max * n_species;
    n_descriptors = (n_radial * (n_radial + 1) / 2) * (l_max + 1);

    // Check the relationship between the power spectrum and beta.
    int beta_check = n_descriptors * (n_descriptors + 1) / 2;
    if (beta_check != beta_size)
        error->all(FLERR,"Beta size doesn't match the number of descriptors.");

    // Set the radial basis.
    if (!strcmp(radial_string, "chebyshev")){
        basis_function = chebyshev;
        radial_hyps = std::vector<double> {0, cutoff};
    }

    // Set the cutoff function.
    if (!strcmp(cutoff_string, "quadratic")) cutoff_function =
        quadratic_cutoff;
    else if (!strcmp(cutoff_string, "cosine")) cutoff_function =
        cos_cutoff;

    // Parse the beta vectors.
    memory->create(beta, beta_size * n_species, "pair:beta");
    if (me == 0) grab(fptr, beta_size * n_species, beta);
    MPI_Bcast(beta, beta_size * n_species, MPI_DOUBLE, 0, world);

}


/* ----------------------------------------------------------------------
   grab n values from file fp and put them in list
   values can be several to a line
   only called by proc 0
------------------------------------------------------------------------- */

void PairFLARE::grab(FILE *fptr, int n, double *list)
{
  char *ptr;
  char line[MAXLINE];

  int i = 0;
  while (i < n) {
    fgets(line,MAXLINE,fptr);
    ptr = strtok(line," \t\n\r\f");
    list[i++] = atof(ptr);
    while ((ptr = strtok(NULL," \t\n\r\f"))) list[i++] = atof(ptr);
  }
}
