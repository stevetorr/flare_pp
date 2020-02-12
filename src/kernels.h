#ifndef KERNELS_H
#define KERNELS_H

#include <vector>
#include <Eigen/Dense>

class LocalEnvironment;
class StructureDescriptor;

class Kernel{
    public:
        std::vector<double> kernel_hyperparameters;

        Kernel();

        Kernel(std::vector<double> kernel_hyperparameters);

        virtual double env_env(const LocalEnvironment & env1,
                               const LocalEnvironment & env2) = 0;

        virtual Eigen::VectorXd env_struc(const LocalEnvironment & env1,
            const StructureDescriptor & struc1) = 0;
};

class DotProductKernel : public Kernel{
    public:
        double signal_variance, power, sig2;

        DotProductKernel();

        // Hyperparameters: [signal_variance, power]
        DotProductKernel(std::vector<double> kernel_hyperparameters);

        double env_env(const LocalEnvironment & env1,
                       const LocalEnvironment & env2);

        Eigen::VectorXd env_struc(const LocalEnvironment & env1,
                                  const StructureDescriptor & struc1);

};

class TwoBodyKernel{
    public:
        double ls, ls1, ls2;
        void (*cutoff_pointer)(double *, double, double, std::vector<double>);
        std::vector<double> cutoff_hyps;

        TwoBodyKernel();

        TwoBodyKernel(double ls, const std::string & cutoff_function,
                      std::vector<double> cutoff_hyps);

        double env_env(const LocalEnvironment & env1,
                       const LocalEnvironment & env2);
        
        Eigen::VectorXd env_struc(const LocalEnvironment & env1,
                                  const StructureDescriptor & struc1);
};

class ThreeBodyKernel{
    public:
        double ls, ls1, ls2;
        void (*cutoff_pointer)(double *, double, double, std::vector<double>);
        std::vector<double> cutoff_hyps;

        ThreeBodyKernel();
        
        ThreeBodyKernel(double ls, const std::string & cutoff_function,
                        std::vector<double> cutoff_hyps);

        double env_env(const LocalEnvironment & env1,
                       const LocalEnvironment & env2);

        Eigen::VectorXd env_struc(const LocalEnvironment & env1,
                                  const StructureDescriptor & struc1);

        void update_kernel_vector(Eigen::VectorXd & kernel_vector,
            int no_elements, int i, double vol_inv,
            double r11, double r22, double r33,
            double fi, double fj, double fdjx1, double fdjx2,
            double fdjy1, double fdjy2, double fdjz1, double fdjz2,
            double xrel1, double xval1, double xrel2, double xval2,
            double yrel1, double yval1, double yrel2, double yval2,
            double zrel1, double zval1, double zrel2, double zval2);
};

#endif