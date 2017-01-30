#include "../ciglet.h"

int noplot = 0;

static void test_statistics() {
  FP_TYPE* rand1 = white_noise(1, 10000);
  FP_TYPE* rand2 = white_noise(1, 10000);
  printf("corr = %f, mean = %f, median = %f\n", corr(rand1, rand2, 10000),
    meanfp(rand1, 10000), medianfp(rand1, 10000));
  free(rand1);
  free(rand2);
}

static void print_mat(FP_TYPE* A, int m, int n) {
  for(int i = 0; i < m; i ++) {
    for(int j = 0; j < n; j ++)
      printf("%6.3f, ", A[i + j * n]);
    printf("\n");
  }
}

static void test_la() {
  int n = 5;
  FP_TYPE A[25] = { // magic(5)
    17, 23, 4, 10, 11,
    24, 5, 6, 12, 18,
    1, 7, 13, 19, 25,
    8, 14, 20, 21, 2,
    15, 16, 22, 3, 9
  };
  print_mat(A, n, n);
  FP_TYPE* Acpy = calloc(n * n, sizeof(FP_TYPE));
  for(int i = 0; i < n * n; i ++) Acpy[i] = A[i];
  int* permidx = ppivot(A, n);
  printf("Permutation: ");
  for(int i = 0; i < n; i ++)
    printf("%d ", permidx[i]);
  printf("\n");
  print_mat(A, n, n);
  printf("permm:\n");
  permm(Acpy, permidx, n, n);
  print_mat(Acpy, n, n);
  lu(A, n);
  printf("LU:\n");
  print_mat(A, n, n);

  free(Acpy);
  free(permidx);
}

static void test_numerical() {
  int order = 20;
  FP_TYPE* a = calloc(order + 1, sizeof(FP_TYPE));
  for(int i = 0; i < order + 1; i ++)
    a[i] = randn(0, 5);

  cplx* r = rootsr(a, order + 1);
  for(int i = 0; i < order; i ++) {
    cplx y = polyvalr(a, order + 1, r[i]);
    printf("roots[%d] = %f + %fi\n", i, r[i].real, r[i].imag);
    printf("f(roots[%d]) = %f + %fi\n", i, y.real, y.imag);
  }
  free(r);
  free(a);
}

static void test_lf() {
  lfmodel testlf = lfmodel_from_rd(1, 0.008, 0.3);
  FP_TYPE* freq = linspace(0, 6000, 200);
  FP_TYPE* lfmagnresp = lfmodel_spectrum(testlf, freq, 200, NULL);
  for(int i = 0; i < 200; i ++)
    lfmagnresp[i] = log(lfmagnresp[i]);
  FP_TYPE* lfperiod = lfmodel_period(testlf, 44100, 500);

  if(! noplot) {
    figure* lffg = plotopen();
    plot(lffg, freq, lfmagnresp, 200, 'b');
    plotclose(lffg);
    lffg = plotopen();
    plot(lffg, NULL, lfperiod, 500, 'b');
    plotclose(lffg);
  }
  free(freq);
  free(lfmagnresp);
  free(lfperiod);
}

static FP_TYPE* test_wav(int* fs, int* nx, int* nbit) {
  int ny;
  FP_TYPE* x = wavread("test/in.wav", fs, nbit, nx);
  FP_TYPE* y = rresample(x, *nx, 1.5, & ny);
  FP_TYPE* y2 = moving_avg(y, ny, 10);
  FP_TYPE* y2d = diff(y2, ny);
  FP_TYPE* y2c = cumsum(y2d, ny);
  free(y);
  free(y2);
  free(y2d);

  for(int i = 0; i < ny; i ++)
    y2c[i] += randn(0, 0.01 * 0.01);
  wavwrite(y2c, ny, *fs * 1.5, *nbit, "test/out-resample-mavg.wav");
  free(y2c);
  return x;
}

static void test_if(FP_TYPE* x, int nx, int fs) {
  ifdetector* ifd = cig_create_ifdetector(220.0 / fs, 220.0 / fs);
  int nhop = 256;
  int nfrm = nx / nhop;
  
  FP_TYPE* x_if = calloc(nfrm, sizeof(FP_TYPE));
  for(int i = 0; i < nfrm; i ++) {
    FP_TYPE* xi = fetch_frame(x, nx, i * nhop, ifd -> nh);
    x_if[i] = cig_ifdetector_estimate(ifd, xi, ifd -> nh) * fs;
    //printf("%f %f\n", (FP_TYPE)i * nhop / fs, x_if[i]);
    free(xi);
  }
  
  if(! noplot) {
    figure* fg = plotopen();
    plot(fg, NULL, x_if, nfrm, 'b');
    plotclose(fg);
  }
  free(x_if);
  cig_delete_ifdetector(ifd);
}

static void test_lpc(FP_TYPE* x, int nx, int fs) {
  int nhop = 256;
  int nfft = 1024;
  int nfrm = round(nx / nhop);
  int order = 12;
  FP_TYPE maxfreq = 5500;
  FP_TYPE normfc = 0;
  FP_TYPE** Xm = malloc2d(nfrm, nfft / 2 + 1, sizeof(FP_TYPE));
  stft(x, nx, nhop, nfrm, 4, 1, & normfc, NULL, Xm, NULL);

  FP_TYPE* faxis = linspace(0, fs / 2, nfft / 2 + 1);
  FP_TYPE* faxis_warp = linspace(0, maxfreq, nfft / 2 + 1);
  FP_TYPE* buffer = malloc(nfft * 2 * sizeof(FP_TYPE));
  for(int i = 0; i < nfrm; i ++) {
    FP_TYPE* Xwarp = interp1(faxis, Xm[i], nfft / 2 + 1, faxis_warp, nfft / 2 + 1);
    FP_TYPE* R = NULL;
    FP_TYPE* a = flpc(Xwarp, nfft / 2 + 1, order, & R);
    free(Xwarp);

    FP_TYPE g = lpgain(a, R, order + 1);
    FP_TYPE* S = lpspec(a, g, order + 1, nfft);
    for(int j = 0; j < nfft / 2 + 1; j ++)
      Xm[i][j] = log(S[j]);

    int npole = 0;
    cplx* poles = calloc(order, sizeof(cplx));
    FP_TYPE* formants = lpresf(a, order + 1, poles, & npole);
    if(i % (nfrm / 10) == 0)
    for(int j = 0; j < npole; j ++) {
      formants[j] *= maxfreq;
      printf("%d %f (%f, %f)\n", j, formants[j], poles[j].real, poles[j].imag);
    }

    free(poles);
    free(formants);

    free(S);
    free(a);
    free(R);
  }
  if(! noplot) {
    figure* fig = plotopen();
    imagesc(fig, Xm, nfrm, nfft / 2 + 1);
    plotclose(fig);
  }
  free(faxis);
  free(faxis_warp);
  free2d(Xm, nfrm);
  free(buffer);
}

static void test_lpcwave(FP_TYPE* x, int nx, int fs) {
  int nhop = 256;
  int nwin = 1024;
  int nfft = 1024;
  int nfrm = round(nx / nhop);
  int p = 32;
  FP_TYPE** Xm = malloc2d(nfrm, nfft / 2 + 1, sizeof(FP_TYPE));
  for(int i = 0; i < nfrm; i ++) {
    FP_TYPE* xi = fetch_frame(x, nx, i * nhop, nwin);
    FP_TYPE* R = NULL;
    FP_TYPE* a = lpc(xi, nwin, p, & R);
    FP_TYPE g = lpgain(a, R, p + 1);
    FP_TYPE* S = lpspec(a, g, p + 1, nfft);
    for(int j = 0; j < nfft / 2 + 1; j ++)
      Xm[i][j] = log(S[j]);
    free(R);
    free(S);
    free(a);
    free(xi);
  }
  if(! noplot) {
    figure* fig = plotopen();
    imagesc(fig, Xm, nfrm, nfft / 2 + 1);
    plotclose(fig);
  }
  free2d(Xm, nfrm);
}

static void test_correlogram(FP_TYPE* x, int nx, int fs) {
  int nhop = 128;
  int nfrm = nx / nhop;
  int max_period = 1024;
  int* center = calloc(nfrm, sizeof(int));
  int* nwin   = calloc(nfrm, sizeof(int));
  for(int i = 0; i < nfrm; i ++) {
    center[i] = i * nhop;
    nwin[i] = 300;
  }
  FP_TYPE** R = malloc2d(nfrm, 1024, sizeof(FP_TYPE));
  cig_correlogram(x, nx, center, nwin, nfrm, max_period, CIG_CORR_ACF, R);
  FP_TYPE* faxis = linspace(0, 999, 1000);
  FP_TYPE** Ri = cig_invcrgm(R, nfrm, max_period, fs, faxis, 1000);
  if(! noplot) {
    figure* fig = plotopen();
    imagesc(fig, Ri, nfrm, 1000);
    plotclose(fig);
  }
  free2d(Ri, nfrm);
  free2d(R, nfrm);
  free(faxis);
  free(center); free(nwin);
}

static void test_spectral(FP_TYPE* x, int nx, int fs, int nbit) {
  int nhop = 256;
  int nfft = 2048;
  int nfrm = round(nx / nhop);
  FP_TYPE normfc = 0;
  FP_TYPE** Xm = malloc2d(nfrm, nfft / 2 + 1, sizeof(FP_TYPE));
  FP_TYPE** Xp = malloc2d(nfrm, nfft / 2 + 1, sizeof(FP_TYPE));
  stft(x, nx, nhop, nfrm, 8, 1, & normfc, NULL, Xm, Xp);

  filterbank* mfbank = create_melfilterbank(nfft / 2 + 1, fs / 2, 36, 50, 8000);
  FP_TYPE** Xmfb = filterbank_spgm(mfbank, Xm, nfrm, nfft, fs, 0);
  FP_TYPE** Xmfcc = calloc(nfrm, sizeof(FP_TYPE*));
  for(int i = 0; i < nfrm; i ++)
    Xmfcc[i] = be2cc(Xmfb[i], 36, 12, 0);

  figure* fg = NULL;
  if(! noplot) {
    fg = plotopen();
    imagesc(fg, Xmfcc, nfrm, 12);
    plotclose(fg);
  }

  printf("Selected content of Xmfcc:\n");
  for(int i = 0; i < nfrm; i += nfrm / 5) {
    for(int j = 0; j < 12; j += 3)
      printf("%10f ", Xmfcc[i][j]);
    puts("");
  }

  FP_TYPE** Xmmf = calloc(nfft / 2 + 1, sizeof(FP_TYPE*));
  FP_TYPE** Xmtr = transpose(Xm, nfrm, nfft / 2 + 1, sizeof(FP_TYPE));
  for(int i = 0; i < nfft / 2 + 1; i ++) {
    Xmmf[i] = medfilt1(Xmtr[i], nfrm, 25);
    for(int j = 0; j < nfrm; j ++)
      Xmmf[i][j] = log_2(Xmmf[i][j]);
  }
  free2d(Xmtr, nfft / 2 + 1);
  Xmtr = transpose(Xmmf, nfft / 2 + 1, nfrm, sizeof(FP_TYPE));
  free2d(Xmmf, nfft / 2 + 1);
  for(int i = 0; i < nfrm; i ++) {
    FP_TYPE* tmp = medfilt1(Xmtr[i], nfft / 2 + 1, 25);
    free(Xmtr[i]);
    Xmtr[i] = tmp;
  }
  free2d(Xmtr, nfrm);

  free2d(Xmfb, nfrm);
  free2d(Xmfcc, nfrm);
  delete_filterbank(mfbank);

  FP_TYPE** Xm2 = copy2d(Xm, nfrm, nfft / 2 + 1, sizeof(FP_TYPE));
  for(int i = 0; i < nfrm; i ++) {
    for(int j = round(30 + pow(sin(i * 0.05), 2) * 200); j < nfft / 2 + 1; j ++)
      Xm2[i][j] = 0;
  }

  int ny;
  FP_TYPE* y = istft(Xm2, Xp, nhop, nfrm, 8, 1, normfc, & ny);
  wavwrite(y, ny, fs, nbit, "test/out-stft-wow.wav");
  for(int i = 0; i < ny; i += ny / 20)
    printf("%10f ", y[i]);
  puts("");
  free(y);
  free2d(Xm2, nfrm);
  printf("Selected content of y:\n");

  FP_TYPE** C = malloc2d(nfrm, nfft / 2 + 1, sizeof(FP_TYPE));
  for(int i = 0; i < nfrm; i ++) {
    free(spec2env(Xm[i], nfft, 250.0 / fs, C[i]));
  }
  FP_TYPE** Xm3 = cegm2spgm(C, nfrm, nfft, nfft / 2 + 1);
  if(! noplot) {
    fg = plotopen();
    imagesc(fg, Xm3, nfrm, nfft / 2 + 1);
    plotclose(fg);
  }

  FP_TYPE* Xm3_flattened = flatten(Xm3, nfrm, nfft / 2 + 1, sizeof(FP_TYPE));
  FP_TYPE Xm3median = medianfp(Xm3_flattened, nfrm * (nfft / 2 + 1));
  FP_TYPE Xm3mean = meanfp(Xm3_flattened, nfrm * (nfft / 2 + 1));
  printf("Median of spectrogram: %f\n", Xm3median);
  printf("Mean of spectrogram: %f (stdvar: %f)\n", Xm3mean,
    sqrt(varfp(Xm3_flattened, nfrm * (nfft / 2 + 1))));
  //for(int i = 0; i < nfrm * (nfft / 2 + 1); i ++)
  //  Xm3_flattened[i] = exp_2(Xm3_flattened[i]);
  FP_TYPE** Xm4 = reshape(Xm3_flattened, nfrm, nfft / 2 + 1, sizeof(FP_TYPE));
  free(Xm3_flattened);
  free2d(Xm3, nfrm); free2d(Xm4, nfrm); free2d(C, nfrm);

  free2d(Xm, nfrm); free2d(Xp, nfrm);
}

int main(int argc, char* argv[]) {
  int stat_on = 0;
  int la_on = 0;
  int lf_on = 0;
  int if_on = 0;
  int numerical_on = 0;
  int lpc_on = 0;
  int lpcwave_on = 0;
  int corr_on = 0;
  int spec_on = 0;

  if(argc >= 2 && ! strcmp(argv[1], "noplot"))
    noplot = 1;
  if(argc >= 3 && (strcmp(argv[2], "all"))) {
    if(! strcmp(argv[2], "statistics"))
      stat_on = 1;
    else
    if(! strcmp(argv[2], "la"))
      la_on = 1;
    else
    if(! strcmp(argv[2], "lf"))
      lf_on = 1;
    else
    if(! strcmp(argv[2], "if"))
      if_on = 1;
    else
    if(! strcmp(argv[2], "numerical"))
      numerical_on = 1;
    else
    if(! strcmp(argv[2], "lpc"))
      lpc_on = 1;
    else
    if(! strcmp(argv[2], "lpcwave"))
      lpcwave_on = 1;
    else
    if(! strcmp(argv[2], "corr"))
      corr_on = 1;
    else
    if(! strcmp(argv[2], "spec"))
      spec_on = 1;
  } else {
    stat_on = la_on = lf_on = if_on = numerical_on = lpc_on =
      lpcwave_on = corr_on = spec_on = 1;
  }

  if(stat_on)
    test_statistics();
  if(la_on)
    test_la();
  if(lf_on)
    test_lf();

  int fs, nx, nbit;
  FP_TYPE* x = test_wav(& fs, & nx, & nbit);

  if(if_on)
    test_if(x, nx, fs);
  if(numerical_on)
    test_numerical();
  if(lpc_on)
    test_lpc(x, nx, fs);
  if(lpcwave_on)
    test_lpcwave(x, nx, fs);
  if(corr_on)
    test_correlogram(x, nx, fs);
  if(spec_on)
    test_spectral(x, nx, fs, nbit);

  free(x);
  return 0;
}
