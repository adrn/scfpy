#include "gsl/gsl_sf_gamma.h"
#include "gsl/gsl_sf_legendre.h"
#include "gsl/gsl_sf_gegenbauer.h"
#include <math.h>
#include "helpers.h"
#include "scf.h"

void accp_firstc(Config config,
                 double *dblfact, // OUTPUT: length = lmax+1
                 double *twoalpha, // OUTPUT: length = lmax+1
                 double *anltilde, // OUTPUT: length = (nmax+1)*(lmax+1)
                 double *coeflm, // OUTPUT: length = (lmax+1)*(lmax+1)
                 int *lmin, int *lskip, // OUTPUT: integers
                 double *c1, // OUTPUT: length = (nmax+1)*(lmax+1)
                 double *c2, // OUTPUT: length = (nmax+1)*(lmax+1)
                 double *c3) // OUTPUT: length = (nmax+1)
{
    /*
    This code follows the "if (firstc)" block of the original Fortran
    implementation of SCF.

    Will call with:

    int lmin, lskip;
    accp_firstc(..., &dblfact[0], ..., &lmin, &lskip, ...)
    */
    int n,l,m,idx;
    double knl, arggam, deltam0;

    dblfact[1] = 1.;
    for (l=2; l<=config.lmax; l++) {
        dblfact[l] = dblfact[l-1]*(2.*l-1.);
    }

    for (n=0; n<=config.nmax; n++) {
        for (l=0; l <= config.lmax; l++) {
            knl = 0.5*n*(n+4.*l+3.)+(l+1.)*(2.*l+1.);

            idx = getIndex2D(n,l,config.lmax+1);
            anltilde[idx] = -pow(2.,(8.*l+6.)) * gsl_sf_fact(n)*(n+2.*l+1.5);
            anltilde[idx] = anltilde[idx] * pow(gsl_sf_gamma(2*l + 1.5), 2);
            anltilde[idx] = anltilde[idx] / (4.*M_PI*knl*gsl_sf_fact(n+4*l+2));
        }
    }

    for (l=0; l <= config.lmax; l++) {
        twoalpha[l] = 2.*(2.*l+1.5);
        for (m=0; m<=l; m++) {
            deltam0 = 2.;
            if (m == 0)
                deltam0 = 1.;

            idx = getIndex2D(l,m,config.lmax+1);
            coeflm[idx] = (2.*l+1.) * deltam0 * gsl_sf_fact(l-m) / gsl_sf_fact(l+m);
        }
    }

    for (n=1; n<=config.nmax; n++) {
        c3[n] = 1. / (n+1.);
        for (l=0; l<=config.lmax; l++) {
            idx = getIndex2D(n,l,config.lmax+1);
            c1[idx] = 2.0*n + twoalpha[l];
            c2[idx] = n-1.0 + twoalpha[l];
        }
    }

    *lskip = 1;
    if (config.zeroodd || config.zeroeven) {
        *lskip = 2;
    }

    *lmin = 0;
    if (config.zeroeven) {
        *lmin = 1;
    }
}

void accp_LH(Config config, double *xyz, double *mass, int *ibound, // INPUT
             double *sinsum, double *cossum, // INPUT: length = (nmax+1)*(lmax+1)*(lmax+1) (to avoid re-defining)
             double G, int *firstc, // INPUT
             double *dblfact, double *twoalpha, double *anltilde, double *coeflm,
             int *lmin, int *lskip,
             double *c1, double *c2, double *c3,
             double *pot, // OUTPUT: length = nbodies
             double *acc) // OUTPUT: length = 3*nbodies
{
    /*
    */

    int j,k,n,l,m, i1,i2;
    double r, costh, phi, xi;
    double un, unm1, plm1m, plm2m;
    double temp3, temp4, temp5, temp6, ttemp5;
    double ar, ath, aphi, cosp, sinp, phinltil, sinth;
    double clm, dlm, elm, flm;

    // TODO: should I define all of this outside?
    int lmax = config.lmax;
    int nmax = config.nmax;
    double cosmphi[lmax+1], sinmphi[lmax+1];
    double ultrasp[(nmax+1)*(lmax+1)], ultraspt[(nmax+1)*(lmax+1)], ultrasp1[(nmax+1)*(lmax+1)];
    double plm[(lmax+1)*(lmax+1)], dplm[(lmax+1)*(lmax+1)];

    // printf("firstc %d\n", *firstc);
    // printf("lmin lskip %d %d\n", *lmin, *lskip);

    if (*firstc) {
        accp_firstc(config,
                    dblfact, twoalpha, anltilde, coeflm,
                    lmin, lskip, c1, c2, c3);
        *firstc = 0;
    }
    // printf("firstc %d\n", *firstc);
    // printf("lmin lskip %d %d\n", *lmin, *lskip);

    // printf("dblfact %f %f %f %f\n", dblfact[1], dblfact[2], dblfact[3], dblfact[4]);
    // printf("twoalpha %f %f %f %f %f %f\n", twoalpha[0], twoalpha[1],
    //        twoalpha[2], twoalpha[3], twoalpha[4], twoalpha[5]);
    // printf("anltilde %f %f %f %f\n", anltilde[getIndex2D(0,0,lmax+1)],
    //        anltilde[getIndex2D(0,1,lmax+1)], anltilde[getIndex2D(3,2,lmax+1)],
    //        anltilde[getIndex2D(5,4,lmax+1)]);
    // printf("coeflm %f %f %f %f\n", coeflm[getIndex2D(0,0,lmax+1)],
    //        coeflm[getIndex2D(0,1,lmax+1)], coeflm[getIndex2D(3,2,lmax+1)],
    //        coeflm[getIndex2D(2,3,lmax+1)]);
    // printf("c1 %f %f %f %f %f\n", c1[getIndex2D(1,1,lmax+1)],
    //        c1[getIndex2D(1,2,lmax+1)], c1[getIndex2D(3,2,lmax+1)],
    //        c1[getIndex2D(2,3,lmax+1)], c1[getIndex2D(5,4,lmax+1)]);
    // printf("c2 %f %f %f %f %f\n", c2[getIndex2D(1,1,lmax+1)],
    //        c2[getIndex2D(1,2,lmax+1)], c2[getIndex2D(3,2,lmax+1)],
    //        c2[getIndex2D(2,3,lmax+1)], c2[getIndex2D(5,4,lmax+1)]);
    // printf("c3 %f %f %f %f %f %f\n", c3[0], c3[1], c3[2], c3[3], c3[4], c3[5]);
    // Note: I compared the above output with Fortran output and looks good.

    // zero out the coefficients
    for (l=0; l<=lmax; l++) {
        for (m=0; m<=l; m++) {
            for (n=0; n<=nmax; n++) {
                i1 = getIndex3D(n,l,m,lmax+1,lmax+1);
                sinsum[i1] = 0.;
                cossum[i1] = 0.;
            }
        }
    }

    // This loop computes the BFE coefficients for all bound particles
    for (k=0; k<config.n_bodies; k++) {
        // printf("%d\n", k);

        if (ibound[k] > 0) { // skip unbound particles
            j = 3*k; // x,y,z in same 2D array

            r = sqrt(xyz[j]*xyz[j] + xyz[j+1]*xyz[j+1] + xyz[j+2]*xyz[j+2]);
            costh = xyz[j+2] / r;
            phi = atan2(xyz[j+1], xyz[j+0]);
            xi = (r - 1.) / (r + 1.);

            // precompute all cos(m*phi), sin(m*phi)
            for (m=0; m<(lmax+1); m++) {
                cosmphi[m] = cos(m*phi);
                sinmphi[m] = sin(m*phi);
            }
            // printf("cosmphi %f %f %f %f\n", cosmphi[0], cosmphi[1], cosmphi[2], cosmphi[3]);
            // printf("sinmphi %f %f %f %f\n", sinmphi[0], sinmphi[1], sinmphi[2], sinmphi[3]);

            for (l=0; l<=lmax; l++) {
                ultrasp[getIndex2D(0,l,lmax+1)] = 1.;
                ultrasp[getIndex2D(1,l,lmax+1)] = twoalpha[l]*xi;

                un = ultrasp[getIndex2D(1,l,lmax+1)];
                unm1 = 1.0;

                for (n=1; n<nmax; n++) {
                    i1 = getIndex2D(n+1,l,lmax+1);
                    i2 = getIndex2D(n,l,lmax+1);
                    ultrasp[i1] = (c1[i2]*xi*un-c2[i2]*unm1)*c3[n];
                    unm1 = un;
                    un = ultrasp[i1];
                }

                for (n=0; n<=nmax; n++) {
                    i1 = getIndex2D(n,l,lmax+1);
                    ultraspt[i1] = ultrasp[i1] * anltilde[i1];
                }
            }

            // printf("ultrasp %f %f %f %f %f\n", ultrasp[getIndex2D(0,0,lmax+1)], ultrasp[getIndex2D(1,0,lmax+1)],
            // ultrasp[getIndex2D(1,3,lmax+1)], ultrasp[getIndex2D(3,1,lmax+1)], ultrasp[getIndex2D(4,4,lmax+1)]);
            // printf("ultraspt %f %f %f %f %f\n", ultraspt[getIndex2D(0,0,lmax+1)], ultraspt[getIndex2D(1,0,lmax+1)],
            //      ultraspt[getIndex2D(1,3,lmax+1)], ultraspt[getIndex2D(3,1,lmax+1)], ultraspt[getIndex2D(4,4,lmax+1)]);

            for (m=0; m<=lmax; m++) {
                i1 = getIndex2D(m,m,lmax+1);
                plm[i1] = 1.0;
                if (m > 0) {
                    plm[i1] = pow(-1.,m) * dblfact[m] * pow(sqrt(1.-costh*costh), m);
                }

                plm1m = plm[i1];
                plm2m = 0.0;

                for (l=m+1; l<=lmax; l++) {
                    i2 = getIndex2D(l,m,lmax+1);
                    plm[i2] = (costh*(2.*l-1.)*plm1m - (l+m-1.)*plm2m) / (l-m);
                    plm2m = plm1m;
                    plm1m = plm[i2];
                }
            }
            // printf("plm %f %f %f %f %f\n", plm[getIndex2D(0,0,lmax+1)], plm[getIndex2D(1,0,lmax+1)],
            //        plm[getIndex2D(1,3,lmax+1)], plm[getIndex2D(3,1,lmax+1)], plm[getIndex2D(4,4,lmax+1)]);

            for (l=(*lmin); l<=lmax; l=l+(*lskip)) {
                temp5 = pow(r,l) / pow(1.+r,2*l+1) * mass[k];
                // printf("temp5 %f %f %d %f\n", temp5, r, l, mass[k]);

                for (m=0; m<=l; m++) {
                    i1 = getIndex2D(l,m,lmax+1);
                    ttemp5 = temp5*plm[i1]*coeflm[i1];
                    temp3 = ttemp5 * sinmphi[m];
                    temp4 = ttemp5 * cosmphi[m];

                    for (n=0; n<=nmax; n++) {
                        i1 = getIndex3D(n,l,m,lmax+1,lmax+1);
                        i2 = getIndex2D(n,l,lmax+1);
                        sinsum[i1] = sinsum[i1] + temp3*ultraspt[i2];
                        cossum[i1] = cossum[i1] + temp4*ultraspt[i2];
                    }
                }
            }

            // printf("sin %e %e %e %e %e %e\n",
            //        sinsum[getIndex3D(0,0,0,lmax+1,lmax+1)],
            //        sinsum[getIndex3D(0,1,1,lmax+1,lmax+1)],
            //        sinsum[getIndex3D(0,0,2,lmax+1,lmax+1)],
            //        sinsum[getIndex3D(0,3,1,lmax+1,lmax+1)],
            //        sinsum[getIndex3D(1,2,4,lmax+1,lmax+1)],
            //        sinsum[getIndex3D(3,0,0,lmax+1,lmax+1)]);
            // printf("cos %e %e %e %e %e %e\n",
            //        cossum[getIndex3D(0,0,0,lmax+1,lmax+1)],
            //        cossum[getIndex3D(0,1,1,lmax+1,lmax+1)],
            //        cossum[getIndex3D(0,0,2,lmax+1,lmax+1)],
            //        cossum[getIndex3D(0,3,1,lmax+1,lmax+1)],
            //        cossum[getIndex3D(1,2,4,lmax+1,lmax+1)],
            //        cossum[getIndex3D(3,0,0,lmax+1,lmax+1)]);
        }
    }

    // This loop computes the acceleration and potential at each particle given the BFE coeffs
    for (k=0; k<config.n_bodies; k++) {
        j = 3*k; // x,y,z in same 2D array

        r = sqrt(xyz[j]*xyz[j] + xyz[j+1]*xyz[j+1] + xyz[j+2]*xyz[j+2]);
        costh = xyz[j+2] / r;
        phi = atan2(xyz[j+1], xyz[j+0]);
        xi = (r - 1.) / (r + 1.);

        // precompute all cos(m*phi), sin(m*phi)
        for (m=0; m<(lmax+1); m++) {
            cosmphi[m] = cos(m*phi);
            sinmphi[m] = sin(m*phi);
        }

        // Zero out potential and accelerations
        pot[k] = 0.;
        ar = 0.;
        ath = 0.;
        aphi = 0.;

        for (l=0; l<=lmax; l++) {
            ultrasp[getIndex2D(0,l,lmax+1)] = 1.;
            ultrasp[getIndex2D(1,l,lmax+1)] = twoalpha[l]*xi;
            ultrasp1[getIndex2D(0,l,lmax+1)] = 0.;
            ultrasp1[getIndex2D(1,l,lmax+1)] = 1.;

            un = ultrasp[getIndex2D(1,l,lmax+1)];
            unm1 = 1.;

            for (n=1; n<nmax; n++) {
                i1 = getIndex2D(n+1,l,lmax+1);
                i2 = getIndex2D(n,l,lmax+1);
                ultrasp[i1] = (c1[i2]*xi*un - c2[i2]*unm1)*c3[n];
                unm1 = un;
                un = ultrasp[i1];
                ultrasp1[i1] = ((twoalpha[l]+(n+1)-1.)*unm1-(n+1)*xi*ultrasp[i1]) / (twoalpha[l]*(1.-xi*xi));
            }
        }

        for (m=0; m<=lmax; m++) {
            i1 = getIndex2D(m,m,lmax+1);
            plm[i1] = 1.0;
            if (m > 0) {
                plm[i1] = pow(-1.,m) * dblfact[m] * pow(sqrt(1.-costh*costh), m);
            }

            plm1m = plm[i1];
            plm2m = 0.0;

            for (l=m+1; l<=lmax; l++) {
                i2 = getIndex2D(l,m,lmax+1);
                plm[i2] = (costh*(2.*l-1.)*plm1m - (l+m-1.)*plm2m) / (l-m);
                plm2m = plm1m;
                plm1m = plm[i2];
            }
        }

        dplm[0,0] = 0.;

        for (l=1; l<=lmax; l++) {
            for (m=0; m<=l; m++) {
                i1 = getIndex2D(l,m,lmax+1);
                if (l == m) {
                    dplm[i1]=l*costh*plm[i1]/(costh*costh-1.0);
                } else {
                    i2 = getIndex2D(l-1,m,lmax+1);
                    dplm[i1]=(l*costh*plm[i1]-(l+m)*plm[i2]) / (costh*costh-1.0);
                }
            }
        }

        for (l=(*lmin); l<=lmax; l=l+(*lskip)) {
            temp3 = 0.;
            temp4 = 0.;
            temp5 = 0.;
            temp6 = 0.;

            for (m=0; m<=l; m++) {
                clm = 0.;
                dlm = 0.;
                elm = 0.;
                flm = 0.;
                for (n=0; n<=nmax; n++) {
                    i1 = getIndex2D(n,l,lmax+1);
                    i2 = getIndex3D(n,l,m,lmax+1,lmax+1);
                    clm = clm + ultrasp[i1]*cossum[i2];
                    dlm = dlm + ultrasp[i1]*sinsum[i2];
                    elm = elm + ultrasp1[i1]*cossum[i2];
                    flm = flm + ultrasp1[i1]*sinsum[i2];
                }

                i1 = getIndex2D(l,m,lmax+1);
                temp3 = temp3 + plm[i1]*(clm*cosmphi[m]+dlm*sinmphi[m]);
                temp4 = temp4 - plm[i1]*(elm*cosmphi[m]+flm*sinmphi[m]);
                // printf("temp4 %e %e %e %d %d %d\n", temp4, elm, flm, n, l, m);
                temp5 = temp5 - dplm[i1]*(clm*cosmphi[m]+dlm*sinmphi[m]);
                // printf("temp5 %e %e %e %e %d %d %d\n", temp5, dplm[i1], clm, dlm, n, l, m);
                temp6 = temp6 - m*plm[i1]*(dlm*cosmphi[m]-clm*sinmphi[m]);
            }

            phinltil = pow(r,l) / pow(1.+r, 2*l+1);
            pot[k] = pot[k] + temp3*phinltil;
            ar = ar + phinltil*(-temp3*(l/r-(2.*l+1.)/(1.+r)) + temp4*4.*(2.*l+1.5)/pow(1.+r,2));
            ath = ath + temp5*phinltil;
            aphi = aphi + temp6*phinltil;
        }

        cosp = cos(phi);
        sinp = sin(phi);

        sinth = sqrt(1.-costh*costh);
        ath = -sinth*ath/r;
        aphi = aphi/(r*sinth);

        acc[j+0] = G*(sinth*cosp*ar + costh*cosp*ath - sinp*aphi);
        acc[j+1] = G*(sinth*sinp*ar + costh*sinp*ath + cosp*aphi);
        acc[j+2] = G*(costh*ar - sinth*ath);
        pot[k] = pot[k]*G;
    }

}

void accp_external(Config config, double *xyz,
                   double *pot, double *acc, double strength) {

    double rs = 10./config.ru;
    double vcirc2 = 220.*220./config.vu/config.vu;
    double GMs = 10.0*vcirc2/config.ru;

    int j, k;
    double r2, rad, tsrad;

    for (k=0; k<config.n_bodies; k++) {
        // THIS IS JUST HERNQUIST
        j = 3*k;
        r2 = xyz[j]*xyz[j] + xyz[j+1]*xyz[j+1] + xyz[j+2]*xyz[j+2];
        rad = sqrt(rad);
        tsrad = GMs/(rad+rs)/(rad+rs)/rad;

        acc[j]   = acc[j]   - strength*tsrad*xyz[j];
        acc[j+1] = acc[j+1] - strength*tsrad*xyz[j+1];
        acc[j+2] = acc[j+2] - strength*tsrad*xyz[j+2];
    }
}

void acc_pot(Config config, int selfgrav, double extern_strength,
             double *xyz, double *mass, int *ibound, // INPUT
             double *sinsum, double *cossum, // INPUT: length = (nmax+1)*(lmax+1)*(lmax+1) (to avoid re-defining)
             double G, int *firstc, // INPUT
             double *dblfact, double *twoalpha, double *anltilde, double *coeflm,
             int *lmin, int *lskip,
             double *c1, double *c2, double *c3,
             double *pot, // OUTPUT: length = nbodies
             double *acc) // OUTPUT: length = 3*nbodies
{
    int j,k;

    if (selfgrav) {
        accp_LH(config, xyz, mass, ibound,
                sinsum, cossum, G, firstc,
                dblfact, twoalpha, anltilde, coeflm, lmin, lskip,
                c1, c2, c3, pot, acc);

        accp_external(config, xyz, pot, acc, extern_strength);

    } else {
        for (k=0; k<config.n_bodies; k++) {
            j = 3*k;
            acc[j+0] = 0.;
            acc[j+1] = 0.;
            acc[j+2] = 0.;
            pot[k] = 0.;
        }
        accp_external(config, xyz, pot, acc, extern_strength);
    }

}

void frame(Config config, int iter,
           double *xyz, double *vxyz, double *mass, double *pot,
           int *pot_idx, double *xyz_frame, double *vxyz_frame) {
    /*
    Shift the phase-space coordinates to be centered on the minimum potential.
    The initial value is the input position and velocity of the progenitor system.

    Parameters
    ----------
    iter : int
        The index of the current iteration (starting from 0).
    n_recenter : int
        After how many steps should we recenter the ...
    n_bodies : int
        Number of particles.

    pot_idx : int* (array, length=n_bodies)
        Index array to sort particles based on potential value.
    */

    // take the most bound particles (?)
    int nend = config.n_bodies / 100;
    if (nend < 128) nend = 128;
    int i,j,k;

    double mred = 0.;
    double xyz_min[3];
    xyz_min[0] = 0.;
    xyz_min[1] = 0.;
    xyz_min[2] = 0.;

    if ((iter == 0) || ((iter % config.n_recenter) == 0)) {
        indexx(config.n_bodies, pot, pot_idx);
    }

    for (i=0; i<nend; i++) {
        j = pot_idx[i];
        k = getIndex2D(j,0,3);
        xyz_min[0] = xyz_min[0] + mass[j]*xyz[k];
        xyz_min[1] = xyz_min[1] + mass[j]*xyz[k+1];
        xyz_min[2] = xyz_min[2] + mass[j]*xyz[k+2];
        mred = mred + mass[j];
    }

    xyz_min[0] = xyz_min[0] / mred;
    xyz_min[1] = xyz_min[1] / mred;
    xyz_min[2] = xyz_min[2] / mred;

    // Update frame and shift to center on the minimum of the potential
    xyz_frame[0] = xyz_frame[0] + xyz_min[0];
    xyz_frame[1] = xyz_frame[1] + xyz_min[1];
    xyz_frame[2] = xyz_frame[2] + xyz_min[2];

    for (i=0; i<config.n_bodies; i++) {
        k = getIndex2D(i,0,3);
        xyz[k] = xyz[k] - xyz_min[0];
        xyz[k+1] = xyz[k+1] - xyz_min[1];
        xyz[k+2] = xyz[k+2] - xyz_min[2];
    }

    // For output, find velocity frame too
    if ((iter % config.n_snapshot) == 0) {
        vxyz_frame[0] = 0.;
        vxyz_frame[1] = 0.;
        vxyz_frame[2] = 0.;
        for (i=0; i<nend; i++) {
            j = pot_idx[i];
            k = getIndex2D(j,0,3);
            vxyz_frame[0] = vxyz_frame[0] + mass[j]*vxyz[k];
            vxyz_frame[1] = vxyz_frame[1] + mass[j]*vxyz[k+1];
            vxyz_frame[2] = vxyz_frame[2] + mass[j]*vxyz[k+2];
        }
        vxyz_frame[0] = vxyz_frame[0] / mred;
        vxyz_frame[1] = vxyz_frame[1] / mred;
        vxyz_frame[2] = vxyz_frame[2] / mred;
    }
}

void initvel(Config config, double *tnow, double *tvel, double dt,
             double *vxyz, double *acc) {
    int j,k;

    for (k=0; k<config.n_bodies; k++) {
        j = 3*k;
        vxyz[j+0] = vxyz[j+0] + 0.5*dt*acc[j+0];
        vxyz[j+1] = vxyz[j+1] + 0.5*dt*acc[j+1];
        vxyz[j+2] = vxyz[j+2] + 0.5*dt*acc[j+2];
    }
    *tvel = *tvel + 0.5*dt;
    *tnow = *tvel;
}

void step_pos(Config config,
              double *xyz, double *vxyz, double dt,
              double *tnow, double *tpos) {
    int j,k;

    for (k=0; k<config.n_bodies; k++) {
        j = 3*k;

        xyz[j]   = xyz[j]   + vxyz[j]*dt;
        xyz[j+1] = xyz[j+1] + vxyz[j+1]*dt;
        xyz[j+2] = xyz[j+2] + vxyz[j+2]*dt;
    }
    *tpos = *tpos + dt;
    *tnow = *tpos;
}

void step_vel(Config config,
              double *vxyz, double *acc, double dt,
              double *tnow, double *tvel) {
    int j,k;

    for (k=0; k<config.n_bodies; k++) {
        j = 3*k;

        vxyz[j]   = vxyz[j]   + acc[j]*dt;
        vxyz[j+1] = vxyz[j+1] + acc[j+1]*dt;
        vxyz[j+2] = vxyz[j+2] + acc[j+2]*dt;
    }
    *tvel = *tvel + dt;
    *tnow = *tvel;
}

// void tidal_start(Config config, double *xyz, double *vxyz, double *mass,
//                  double dt, double *tnow, double *tpos, double *tvel) {
//     double v_cm[3], a_cm[3], mtot, t_tidal, strength;
//     int i,j,k,n;

//     for (n=0; n<config.n_tidal; n++) {

//         // Advance position by one step
//         step_pos(config, xyz, vxyz, dt, tnow, tpos)

//         // Find center-of-mass vel. and acc.
//         for (i=0; i<3; i++) {
//             v_cm[i] = 0.;
//             a_cm[i] = 0.;
//         }

//         for (k=0; k<config.n_bodies; k++) {
//             j = 3*k;
//             mtot = mtot + mass[k];

//             v_cm[j]   = v_cm[j]   + mass[k]*vxyz[j];
//             v_cm[j+1] = v_cm[j+1] + mass[k]*vxyz[j+1];
//             v_cm[j+2] = v_cm[j+2] + mass[k]*vxyz[j+2];

//             a_cm[j]   = a_cm[j]   + mass[k]*acc[j];
//             a_cm[j+1] = a_cm[j+1] + mass[k]*acc[j+1];
//             a_cm[j+2] = a_cm[j+2] + mass[k]*acc[j+2];
//         }

//         v_cm[j]   = v_cm[j]   / mtot
//         v_cm[j+1] = v_cm[j+1] / mtot
//         v_cm[j+2] = v_cm[j+2] / mtot
//         a_cm[j]   = a_cm[j]   / mtot
//         a_cm[j+1] = a_cm[j+1] / mtot
//         a_cm[j+2] = a_cm[j+2] / mtot

//         // Retard position and velocity by one step relative to center of mass??
//         for (k=0; k<config.n_bodies; k++) {
//             j = 3*k;

//             xyz[j] = xyz[j] - v_cm[j]*dt;
//             xyz[j+1] = xyz[j+1] - v_cm[j+1]*dt;
//             xyz[j+2] = xyz[j+2] - v_cm[j+2]*dt;

//             vxyz[j] = vxyz[j] - a_cm[j]*dt;
//             vxyz[j+1] = vxyz[j+1] - a_cm[j+1]*dt;
//             vxyz[j+2] = vxyz[j+2] - a_cm[j+2]*dt;
//         }

//         // Increase tidal field
//         t_tidal = ((double)n) / ((double)config.n_tidal);
//         strength = (-2.*t_tidal + 3.)*t_tidal*t_tidal;

//         // Find new accelerations
//         acc_pot(config, selfgrav, strength, xyz, mass,
//                 ibound, sinsum, cossum, G, firstc,
//                 dblfact, twoalpha, anltilde, coeflm,
//                 lmin, lskip, c1, c2, c3, pot, acc);

//         // Advance velocity by one step
//         step_pos(config, vxyz, acc, dt, tnow, tvel)

//         // Reset times
//         *tvel = 0.5*dt;
//         *tpos = 0.;
//         *tnow = 0.
//     }


// }
