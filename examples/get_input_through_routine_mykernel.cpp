#include "bbfmm3d.hpp"

void SetMetaData(double& L, int& n, doft& dof, int& Ns, int& Nf, int& m, int& level,double& eps) {
    L       = 1;    // Length of simulation cell (assumed to be a cube)
    n       = 3;    // Number of Chebyshev nodes per dimension
    dof.f   = 1;
    dof.s   = 1;
    Ns      = 1e4;  // Number of sources in simulation cell
    Nf      = 1e4;  // Number of field points in simulation cell
    m       = 1;
    level   = 3;
    eps     = 1e-5;
}

/*
 * Function: SetSources
 * -------------------------------------------------------------------
 * Distributes an equal number of positive and negative charges uniformly
 * in the simulation cell ([-0.5*L,0.5*L])^3 and take these same locations
 * as the field points.
 */

void SetSources(vector3 *field, int Nf, vector3 *source, int Ns, double *q, int m,
                doft *dof, double L) {
	
	int l, i, j, k=0;
	
    // Distributes the sources randomly uniformly in a cubic cell
    for (l=0;l<m;l++) {
        for (i=0;i<Ns;i++) {
            for (j=0; j<dof->s; j++, k++){
                q[k] = frand(0,1);
            }
        }
        
    }
    
    for (i=0;i<Ns;i++) {
		
		source[i].x = frand(-0.5,0.5)*L;
		source[i].y = frand(-0.5,0.5)*L;
		source[i].z = frand(-0.5,0.5)*L;

	}
    
    // Randomly set field points
	for (i=0;i<Nf;i++) {

		field[i].x = frand(-0.5,0.5)*L;
		field[i].y = frand(-0.5,0.5)*L;
		field[i].z = frand(-0.5,0.5)*L;
    }

}


class myKernel: public H2_3D_Tree {
public:
    myKernel(double L, int level, int n,  double epsilon, int use_chebyshev):H2_3D_Tree(L,level,n, epsilon, use_chebyshev){};
    virtual void setHomogen(string& kernelType,doft *dof) {
        homogen = -1;
        symmetry = 1;
        kernelType = "myKernel";
        dof->s = 1;
        dof->f = 1;
    }
    virtual void EvaluateKernel(vector3 fieldpos, vector3 sourcepos,
                                double *K, doft *dof) {
        vector3 diff;        
        // Compute 1/r
        diff.x = sourcepos.x - fieldpos.x;
        diff.y = sourcepos.y - fieldpos.y;
        diff.z = sourcepos.z - fieldpos.z;
        double r = sqrt(diff.x*diff.x + diff.y*diff.y + diff.z*diff.z);
        *K = 1/r;
    }
};

int main(int argc, char *argv[]) {
    /**********************************************************/
    /*                                                        */
    /*              Initializing the problem                  */
    /*                                                        */
    /**********************************************************/
    
    double L;       // Length of simulation cell (assumed to be a cube)
    int n;          // Number of Chebyshev nodes per dimension
    doft dof;
    int Ns;         // Number of sources in simulation cell
    int Nf;         // Number of field points in simulation cell
    int m;
    int level;
    double eps;
    int use_chebyshev = 1;
    
    SetMetaData(L, n, dof, Ns, Nf, m, level, eps);
    
    vector3* source = new vector3[Ns];    // Position array for the source points
    vector3* field = new vector3[Nf];     // Position array for the field points
    double *q = new double[Ns*dof.s*m];  // Source array
    
    SetSources(field,Nf,source,Ns,q,m,&dof,L);


    double err;
    double *stress      =  new double[Nf*dof.f*m];// Field array (BBFMM calculation)
    double *stress_dir  =  new double[Nf*dof.f*m];// Field array (direct O(N^2) calculation)
    
    
    
    /**********************************************************/
    /*                                                        */
    /*                 Fast matrix vector product             */
    /*                                                        */
    /**********************************************************/
    
    /*****      Pre Computation     ******/
    clock_t  t0 = clock();
    myKernel Atree(L,level, n, eps, use_chebyshev);
    Atree.buildFMMTree();
    clock_t t1 = clock();
    double tPre = t1 - t0;
    
    /*****      FMM Computation     *******/
    t0 = clock();
    H2_3D_Compute<myKernel> compute(&Atree, field, source, Ns, Nf, q,m, stress);
    t1 = clock();
    double tFMM = t1 - t0;
    
    /*****      output result to binary file    ******/
    // string outputfilename = "../output/stress.bin";
    // write_Into_Binary_File(outputfilename, stress, m*Nf*dof.f);
    
    /**********************************************************/
    /*                                                        */
    /*              Exact matrix vector product               */
    /*                                                        */
    /**********************************************************/
    t0 = clock();
    DirectCalc3D(&Atree, field, Nf, source, q, m, Ns, 0 , L, stress_dir);
    t1 = clock();
    double tExact = t1 - t0;
    
    cout << "Pre-computation time: " << double(tPre) / double(CLOCKS_PER_SEC) << endl;
    cout << "FMM computing time:   " << double(tFMM) / double(CLOCKS_PER_SEC)  << endl;
    cout << "FMM total time:   " << double(tFMM+tPre) / double(CLOCKS_PER_SEC)  << endl;
    cout << "Exact computing time: " << double(tExact) / double(CLOCKS_PER_SEC)  << endl;
    
    // Compute the 2-norm error
    err = ComputeError(stress,stress_dir,Nf,&dof,m);
    cout << "Relative Error: "  << err << endl;
    
    /*******            Clean Up        *******/
    
    delete []stress;
    delete []stress_dir;
    delete []q;
    delete []source;
    delete []field;
    return 0;
}
