# libc

## add Functional

- [cacosf, cacoshf, casinf, casinhf, catanf, catanhf, ccosf, ccoshf, cexpf, clogf, csinf, csinhf, csqrtf, ctanf, ctanhf and cpowf](../../../core/libs/prx/libc/src/FloatBuiltins.cpp) (libc) - `clogf` computes the log magnitude and phase in double precision, using `log1p` near the unit circle. The other float forms use `std::complex<float>` with the C99 Annex G branch cuts. The console's results at the branch cuts and for special values are unverified.
