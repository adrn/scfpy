void indexx(int n, double *arrin, int *indx) {
    /* Thanks Numerical Recipes! */
    int l,j,ir,indxt,i;
    float q;

    for (j=1;j<=n;j++) indx[j]=j;
    l=(n >> 1) + 1;
    ir=n;
    for (;;) {
        if (l > 1)
            q=arrin[(indxt=indx[--l])];
        else {
            q=arrin[(indxt=indx[ir])];
            indx[ir]=indx[1];
            if (--ir == 1) {
                indx[1]=indxt;
                return;
            }
        }
        i=l;
        j=l << 1;
        while (j <= ir) {
            if (j < ir && arrin[indx[j]] < arrin[indx[j+1]]) j++;
            if (q < arrin[indx[j]]) {
                indx[i]=indx[j];
                j += (i=j);
            }
            else j=ir+1;
        }
        indx[i]=indxt;
    }
}

int getIndex2D(int row, int col, int ncol) {
    return row*ncol + col;
}

int getIndex3D(int row, int col, int dep, int ncol, int ndep) {
    return (row*ncol + col)*ndep + dep;
}
