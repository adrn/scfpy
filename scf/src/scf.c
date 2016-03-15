#include "gsl/gsl_sf_gamma.h"
#include "gsl/gsl_sf_legendre.h"
#include "gsl/gsl_sf_gegenbauer.h"
#include <math.h>
#include "helpers.h"
#include "scf.h"

void accp_firstc(Config config, Placeholders p) {
    /*
    This code follows the "if (firstc)" block of the original Fortran
    implementation of SCF.

    Will call with:

    int lmin, lskip;
    accp_firstc(..., &dblfact[0], ..., &lmin, &lskip, ...)
    */
    int n,l,m,idx;
    double knl, arggam, deltam0;

    p.dblfact[1] = 1.;
    for (l=2; l<=config.lmax; l++) {
        p.dblfact[l] = p.dblfact[l-1]*(2.*l-1.);
    }

    for (n=0; n<=config.nmax; n++) {
        for (l=0; l <= config.lmax; l++) {
            knl = 0.5*n*(n+4.*l+3.)+(l+1.)*(2.*l+1.);

            idx = getIndex2D(n,l,config.lmax+1);
            p.anltilde[idx] = -pow(2.,(8.*l+6.)) * gsl_sf_fact(n)*(n+2.*l+1.5);
            p.anltilde[idx] = p.anltilde[idx] * pow(gsl_sf_gamma(2*l + 1.5), 2);
            p.anltilde[idx] = p.anltilde[idx] / (4.*M_PI*knl*gsl_sf_fact(n+4*l+2));
        }
    }

    for (l=0; l <= config.lmax; l++) {
        p.twoalpha[l] = 2.*(2.*l+1.5);
        for (m=0; m<=l; m++) {
            deltam0 = 2.;
            if (m == 0)
                deltam0 = 1.;

            idx = getIndex2D(l,m,config.lmax+1);
            p.coeflm[idx] = (2.*l+1.) * deltam0 * gsl_sf_fact(l-m) / gsl_sf_fact(l+m);
        }
    }

    for (n=1; n<=config.nmax; n++) {
        p.c3[n] = 1. / (n+1.);
        for (l=0; l<=config.lmax; l++) {
            idx = getIndex2D(n,l,config.lmax+1);
            p.c1[idx] = 2.0*n + p.twoalpha[l];
            p.c2[idx] = n-1.0 + p.twoalpha[l];
        }
    }

    *(p.lskip) = 1;
    if (config.zeroodd || config.zeroeven) {
        *(p.lskip) = 2;
    }

    *(p.lmin) = 0;
    if (config.zeroeven) {
        *(p.lmin) = 1;
    }
}

void accp_LH(Config config, Bodies b, Placeholders p, int *firstc) {
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

    // printf("firstc %d\n", *firstc);
    // printf("lmin lskip %d %d\n", *lmin, *lskip);

    if (*firstc) {
        accp_firstc(config, p);
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
                p.sinsum[i1] = 0.;
                p.cossum[i1] = 0.;
            }
        }
    }

    // This loop computes the BFE coefficients for all bound particles
    for (k=0; k<config.n_bodies; k++) {
        // printf("%d\n", k);

        if (b.ibound[k] > 0) { // skip unbound particles
            r = sqrt(b.x[k]*b.x[k] + b.y[k]*b.y[k] + b.z[k]*b.z[k]);
            costh = b.z[k] / r;
            phi = atan2(b.y[k], b.x[k]);
            xi = (r - 1.) / (r + 1.);

            // precompute all cos(m*phi), sin(m*phi)
            for (m=0; m<(lmax+1); m++) {
                cosmphi[m] = cos(m*phi);
                sinmphi[m] = sin(m*phi);
            }
            // printf("cosmphi %f %f %f %f\n", cosmphi[0], cosmphi[1], cosmphi[2], cosmphi[3]);
            // printf("sinmphi %f %f %f %f\n", sinmphi[0], sinmphi[1], sinmphi[2], sinmphi[3]);

            for (l=0; l<=lmax; l++) {
                p.ultrasp[getIndex2D(0,l,lmax+1)] = 1.;
                p.ultrasp[getIndex2D(1,l,lmax+1)] = p.twoalpha[l]*xi;

                un = p.ultrasp[getIndex2D(1,l,lmax+1)];
                unm1 = 1.0;

                for (n=1; n<nmax; n++) {
                    i1 = getIndex2D(n+1,l,lmax+1);
                    i2 = getIndex2D(n,l,lmax+1);
                    p.ultrasp[i1] = (p.c1[i2]*xi*un-p.c2[i2]*unm1)*p.c3[n];
                    unm1 = un;
                    un = p.ultrasp[i1];
                }

                for (n=0; n<=nmax; n++) {
                    i1 = getIndex2D(n,l,lmax+1);
                    p.ultraspt[i1] = p.ultrasp[i1] * p.anltilde[i1];
                }
            }

            // printf("ultrasp %f %f %f %f %f\n", ultrasp[getIndex2D(0,0,lmax+1)], ultrasp[getIndex2D(1,0,lmax+1)],
            // ultrasp[getIndex2D(1,3,lmax+1)], ultrasp[getIndex2D(3,1,lmax+1)], ultrasp[getIndex2D(4,4,lmax+1)]);
            // printf("ultraspt %f %f %f %f %f\n", ultraspt[getIndex2D(0,0,lmax+1)], ultraspt[getIndex2D(1,0,lmax+1)],
            //      ultraspt[getIndex2D(1,3,lmax+1)], ultraspt[getIndex2D(3,1,lmax+1)], ultraspt[getIndex2D(4,4,lmax+1)]);

            for (m=0; m<=lmax; m++) {
                i1 = getIndex2D(m,m,lmax+1);
                p.plm[i1] = 1.0;
                if (m > 0) {
                    p.plm[i1] = pow(-1.,m) * p.dblfact[m] * pow(sqrt(1.-costh*costh), m);
                }

                plm1m = p.plm[i1];
                plm2m = 0.0;

                for (l=m+1; l<=lmax; l++) {
                    i2 = getIndex2D(l,m,lmax+1);
                    p.plm[i2] = (costh*(2.*l-1.)*plm1m - (l+m-1.)*plm2m) / (l-m);
                    plm2m = plm1m;
                    plm1m = p.plm[i2];
                }
            }
            // printf("plm %f %f %f %f %f\n", plm[getIndex2D(0,0,lmax+1)], plm[getIndex2D(1,0,lmax+1)],
            //        plm[getIndex2D(1,3,lmax+1)], plm[getIndex2D(3,1,lmax+1)], plm[getIndex2D(4,4,lmax+1)]);

            for (l=(*(p.lmin)); l<=lmax; l=l+(*(p.lskip))) {
                temp5 = pow(r,l) / pow(1.+r,2*l+1) * b.mass[k];
                // printf("temp5 %f %f %d %f\n", temp5, r, l, mass[k]);

                for (m=0; m<=l; m++) {
                    i1 = getIndex2D(l,m,lmax+1);
                    ttemp5 = temp5 * p.plm[i1] * p.coeflm[i1];
                    temp3 = ttemp5 * sinmphi[m];
                    temp4 = ttemp5 * cosmphi[m];

                    for (n=0; n<=nmax; n++) {
                        i1 = getIndex3D(n,l,m,lmax+1,lmax+1);
                        i2 = getIndex2D(n,l,lmax+1);
                        p.sinsum[i1] = p.sinsum[i1] + temp3*p.ultraspt[i2];
                        p.cossum[i1] = p.cossum[i1] + temp4*p.ultraspt[i2];
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
        r = sqrt(b.x[k]*b.x[k] + b.y[k]*b.y[k] + b.z[k]*b.z[k]);
        costh = b.z[k] / r;
        phi = atan2(b.y[k], b.x[k]);
        xi = (r - 1.) / (r + 1.);

        // precompute all cos(m*phi), sin(m*phi)
        for (m=0; m<(lmax+1); m++) {
            cosmphi[m] = cos(m*phi);
            sinmphi[m] = sin(m*phi);
        }

        // Zero out potential and accelerations
        b.pot[k] = 0.;
        ar = 0.;
        ath = 0.;
        aphi = 0.;

        for (l=0; l<=lmax; l++) {
            p.ultrasp[getIndex2D(0,l,lmax+1)] = 1.;
            p.ultrasp[getIndex2D(1,l,lmax+1)] = p.twoalpha[l]*xi;
            p.ultrasp1[getIndex2D(0,l,lmax+1)] = 0.;
            p.ultrasp1[getIndex2D(1,l,lmax+1)] = 1.;

            un = p.ultrasp[getIndex2D(1,l,lmax+1)];
            unm1 = 1.;

            for (n=1; n<nmax; n++) {
                i1 = getIndex2D(n+1,l,lmax+1);
                i2 = getIndex2D(n,l,lmax+1);
                p.ultrasp[i1] = (p.c1[i2]*xi*un - p.c2[i2]*unm1) * p.c3[n];
                unm1 = un;
                un = p.ultrasp[i1];
                p.ultrasp1[i1] = ((p.twoalpha[l]+(n+1)-1.)*unm1-(n+1)*xi*p.ultrasp[i1]) / (p.twoalpha[l]*(1.-xi*xi));
            }
        }

        for (m=0; m<=lmax; m++) {
            i1 = getIndex2D(m,m,lmax+1);
            p.plm[i1] = 1.0;
            if (m > 0) {
                p.plm[i1] = pow(-1.,m) * p.dblfact[m] * pow(sqrt(1.-costh*costh), m);
            }

            plm1m = p.plm[i1];
            plm2m = 0.0;

            for (l=m+1; l<=lmax; l++) {
                i2 = getIndex2D(l,m,lmax+1);
                p.plm[i2] = (costh*(2.*l-1.)*plm1m - (l+m-1.)*plm2m) / (l-m);
                plm2m = plm1m;
                plm1m = p.plm[i2];
            }
        }

        p.dplm[0,0] = 0.;

        for (l=1; l<=lmax; l++) {
            for (m=0; m<=l; m++) {
                i1 = getIndex2D(l,m,lmax+1);
                if (l == m) {
                    p.dplm[i1]=l*costh*p.plm[i1]/(costh*costh-1.0);
                } else {
                    i2 = getIndex2D(l-1,m,lmax+1);
                    p.dplm[i1]=(l*costh*p.plm[i1]-(l+m)*p.plm[i2]) / (costh*costh-1.0);
                }
            }
        }

        for (l=(*(p.lmin)); l<=lmax; l=l+(*(p.lskip))) {
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
                    clm = clm + p.ultrasp[i1]*p.cossum[i2];
                    dlm = dlm + p.ultrasp[i1]*p.sinsum[i2];
                    elm = elm + p.ultrasp1[i1]*p.cossum[i2];
                    flm = flm + p.ultrasp1[i1]*p.sinsum[i2];
                }

                i1 = getIndex2D(l,m,lmax+1);
                temp3 = temp3 + p.plm[i1]*(clm*cosmphi[m]+dlm*sinmphi[m]);
                temp4 = temp4 - p.plm[i1]*(elm*cosmphi[m]+flm*sinmphi[m]);
                // printf("temp4 %e %e %e %d %d %d\n", temp4, elm, flm, n, l, m);
                temp5 = temp5 - p.dplm[i1]*(clm*cosmphi[m]+dlm*sinmphi[m]);
                // printf("temp5 %e %e %e %e %d %d %d\n", temp5, p.dplm[i1], clm, dlm, n, l, m);
                temp6 = temp6 - m*p.plm[i1]*(dlm*cosmphi[m]-clm*sinmphi[m]);
            }

            phinltil = pow(r,l) / pow(1.+r, 2*l+1);
            b.pot[k] = b.pot[k] + temp3*phinltil;
            ar = ar + phinltil*(-temp3*(l/r-(2.*l+1.)/(1.+r)) + temp4*4.*(2.*l+1.5)/pow(1.+r,2));
            ath = ath + temp5*phinltil;
            aphi = aphi + temp6*phinltil;
        }

        cosp = cos(phi);
        sinp = sin(phi);

        sinth = sqrt(1.-costh*costh);
        ath = -sinth*ath/r;
        aphi = aphi/(r*sinth);

        b.ax[k] = config.G*(sinth*cosp*ar + costh*cosp*ath - sinp*aphi);
        b.ay[k] = config.G*(sinth*sinp*ar + costh*sinp*ath + cosp*aphi);
        b.az[k] = config.G*(costh*ar - sinth*ath);
        b.pot[k] = b.pot[k]*config.G;
    }

}

void accp_external(Config config, Bodies b, double strength, double *xyz_frame) {

    double rs = 10./config.ru;
    double vcirc2 = 220.*220./config.vu/config.vu;
    double GMs = 10.0*vcirc2/config.ru;

    int j, k;
    double xx, yy, zz, r2, rad, tsrad;

    for (k=0; k<config.n_bodies; k++) {
        // THIS IS JUST HERNQUIST
        xx = b.x[k] + xyz_frame[0];
        yy = b.y[k] + xyz_frame[1];
        zz = b.z[k] + xyz_frame[2];

        r2 = xx*xx + yy*yy + zz*zz;
        rad = sqrt(r2);
        tsrad = GMs/(rad+rs)/(rad+rs)/rad;

        b.ax[k] = b.ax[k] - strength*tsrad*xx;
        b.ay[k] = b.ay[k] - strength*tsrad*yy;
        b.az[k] = b.az[k] - strength*tsrad*zz;

        if (k == 0) {
            // printf("%e\n", strength*tsrad*b.x[k]);
            printf("DERP %e %e %e\n", rad, strength, tsrad);
        }
    }
}

void acc_pot(Config config, double extern_strength,
             Bodies b, Placeholders p, int *firstc, double *xyz_frame) {
    int j,k;

    if (config.selfgravitating) {
        accp_LH(config, b, p, firstc);
        accp_external(config, b, extern_strength, xyz_frame);
    } else {
        for (k=0; k<config.n_bodies; k++) {
            b.ax[k] = 0.;
            b.ay[k] = 0.;
            b.az[k] = 0.;
            b.pot[k] = 0.;
        }
        accp_external(config, b, extern_strength, xyz_frame);
    }

}

void frame(Config config, int iter, Bodies b,
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
        indexx(config.n_bodies, b.pot, pot_idx);
    }

    for (i=0; i<nend; i++) {
        k = pot_idx[i];
        xyz_min[0] = xyz_min[0] + b.mass[k]*b.x[k];
        xyz_min[1] = xyz_min[1] + b.mass[k]*b.y[k];
        xyz_min[2] = xyz_min[2] + b.mass[k]*b.z[k];
        mred = mred + b.mass[k];
    }

    xyz_min[0] = xyz_min[0] / mred;
    xyz_min[1] = xyz_min[1] / mred;
    xyz_min[2] = xyz_min[2] / mred;

    // Update frame and shift to center on the minimum of the potential
    xyz_frame[0] = xyz_frame[0] + xyz_min[0];
    xyz_frame[1] = xyz_frame[1] + xyz_min[1];
    xyz_frame[2] = xyz_frame[2] + xyz_min[2];

    for (k=0; k<config.n_bodies; k++) {
        b.x[k] = b.x[k] - xyz_min[0];
        b.y[k] = b.y[k] - xyz_min[1];
        b.z[k] = b.z[k] - xyz_min[2];
    }

    // For output, find velocity frame too
    if ((iter % config.n_snapshot) == 0) {
        vxyz_frame[0] = 0.;
        vxyz_frame[1] = 0.;
        vxyz_frame[2] = 0.;
        for (i=0; i<nend; i++) {
            k = pot_idx[i];
            vxyz_frame[0] = vxyz_frame[0] + b.mass[k]*b.vx[k];
            vxyz_frame[1] = vxyz_frame[1] + b.mass[k]*b.vy[k];
            vxyz_frame[2] = vxyz_frame[2] + b.mass[k]*b.vz[k];
        }
        vxyz_frame[0] = vxyz_frame[0] / mred;
        vxyz_frame[1] = vxyz_frame[1] / mred;
        vxyz_frame[2] = vxyz_frame[2] / mred;
    }
}

void check_progenitor(Config config, int iter, Bodies b, Placeholders p,
                      double *tnow, double *xyz_frame, double *vxyz_frame) {
    double m_prog, m_safe;
    double vx_rel, vy_rel, vz_rel;
    int k,n;
    int N_MASS_ITER = 30; // TODO: set in config?
    int not_firstc = 0;
    int broke = 0;

    for (k=0; k<config.n_bodies; k++) {
        vx_rel = b.vx[k] - vxyz_frame[0];
        vy_rel = b.vy[k] - vxyz_frame[1];
        vz_rel = b.vz[k] - vxyz_frame[2];
        b.kin[k] = 0.5*(vx_rel*vx_rel + vy_rel*vy_rel + vz_rel*vz_rel);
    }

    // iteratively shave off unbound particles to find prog. mass
    for (n=0; n<N_MASS_ITER; n++) {
        m_safe = m_prog;
        m_prog = 0.;

        for (k=0; k<config.n_bodies; k++) {
            if (b.kin[k] > fabs(b.pot[k])) {
                printf("unbound %d\n", k+1);
                b.ibound[k] = 0;
                if (b.tub[k] == 0) b.tub[k] = *tnow;
            } else {
                m_prog = m_prog + b.mass[k];
            }
        }

        // printf("msafe prog %f %f\n", m_safe, m_prog);
        if (m_safe <= m_prog) {
            broke = 1;
            break;
        }

        // Find new accelerations with unbound stuff removed
        acc_pot(config, 1., b, p, &not_firstc, xyz_frame);
    }

    // if the loop above didn't break, progenitor is dissolved?
    if (broke == 0) m_prog = 0.;

    printf("Found progenitor mass (%d iter): %f\n", n, m_prog);

    if (m_prog == 0) config.selfgravitating = 0;

}

void tidal_start(Config config, Bodies b, Placeholders p,
                 double dt, double *tnow, double *tpos, double *tvel,
                 int *pot_idx, double *xyz_frame, double *vxyz_frame) {
    double v_cm[3], a_cm[3], mtot, t_tidal, strength;
    int i,k,n;
    int not_firstc = 0;

    for (n=0; n<config.n_tidal; n++) {

        // Advance position by one step
        step_pos(config, b, dt, tnow, tpos);

        // Find center-of-mass vel. and acc.
        for (i=0; i<3; i++) {
            v_cm[i] = 0.;
            a_cm[i] = 0.;
        }
        mtot = 0.;

        for (k=0; k<config.n_bodies; k++) {
            v_cm[0] = v_cm[0] + b.mass[k]*b.vx[k];
            v_cm[1] = v_cm[1] + b.mass[k]*b.vy[k];
            v_cm[2] = v_cm[2] + b.mass[k]*b.vz[k];

            a_cm[0] = a_cm[0] + b.mass[k]*b.ax[k];
            a_cm[1] = a_cm[1] + b.mass[k]*b.ay[k];
            a_cm[2] = a_cm[2] + b.mass[k]*b.az[k];

            mtot = mtot + b.mass[k];
        }

        for (i=0; i<3; i++) {
            v_cm[i] = v_cm[i]/mtot;
            a_cm[i] = a_cm[i]/mtot;
        }

        // printf("%e %e %e\n", v_cm[0], v_cm[1], v_cm[2]);

        // Retard position and velocity by one step relative to center of mass??
        for (k=0; k<config.n_bodies; k++) {
            b.x[k] = b.x[k] - v_cm[0]*dt;
            b.y[k] = b.y[k] - v_cm[1]*dt;
            b.z[k] = b.z[k] - v_cm[2]*dt;

            b.vx[k] = b.vx[k] - a_cm[0]*dt;
            b.vy[k] = b.vy[k] - a_cm[1]*dt;
            b.vz[k] = b.vz[k] - a_cm[2]*dt;
        }
        // printf("\n");
        // if (n == 2) {
        //     exit(0);
        // }

        // Increase tidal field
        t_tidal = ((double)n + 1.) / ((double)config.n_tidal);
        strength = (-2.*t_tidal + 3.)*t_tidal*t_tidal;

        // Find new accelerations
        acc_pot(config, strength, b, p, &not_firstc, xyz_frame);

        // Advance velocity by one step
        step_vel(config, b, dt, tnow, tvel);

        // Reset times
        *tvel = 0.5*dt;
        *tpos = 0.;
        *tnow = 0.;

        printf("Tidal start: %d\n", n);
    }

    // Synchronize the velocities with the positions
    step_vel(config, b, -0.5*dt, tnow, tvel);
    frame(config, 0, b, pot_idx, xyz_frame, vxyz_frame);
    check_progenitor(config, 0, b, p, tnow, xyz_frame, vxyz_frame);
    // TODO: outlog?

    // Reset the velocities to being 1/2 step ahead of the positions
    step_vel(config, b, -0.5*dt, tnow, tvel);

    // for (k=0; k<4; k++) {
    //     printf("%d\n", k+1);
    //     printf("xyz %e %e %e\n", b.x[k], b.y[k], b.z[k]);
    //     printf("vxyz %e %e %e\n", b.vx[k], b.vy[k], b.vz[k]);
    // }
    // exit(0);

}
