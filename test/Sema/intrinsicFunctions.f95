! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s
PROGRAM intrinfuntest
  INTEGER i
  REAL r
  DOUBLE PRECISION d
  COMPLEX c
  CHARACTER (LEN=100) string
  LOGICAL logicalResult

  integer (kind=8) i64
  real (kind =8) r8
  complex(kind = 8) c8

  INTRINSIC INT, IFIX, IDINT
  INTRINSIC REAL, FLOAT, sngl
  INTRINSIC DBLE, cmplx
  INTRINSIC char, ICHAR

  INTRINSIC AINT, dint, anint, DNINT, nint, IDNINT, ceiling, floor
  INTRINSIC abs, iabs, dabs, cabs
  INTRINSIC mod, sign, dim, dprod, max, min
  INTRINSIC len, len_trim, index
  INTRINSIC aimag, conjg

  intrinsic sqrt, dsqrt, csqrt, exp, dexp, cexp
  intrinsic log, alog, dlog, clog, log10, alog10, dlog10
  intrinsic sin, dsin, csin, cos, dcos, ccos, tan, dtan
  intrinsic asin, dasin, acos, dacos, atan, datan, atan2, datan2
  intrinsic sinh, dsinh, cosh, dcosh, tanh, dtanh

  intrinsic lge, lgt, lle, llt

!! conversion functions

  i = INT(2.0) ! CHECK: i = int(2)
  i = int(2.0D1) ! CHECK: i = int(20)
  i = INT((1,2)) ! CHECK: i = int((real(1),real(2)))
  i = ifix(1.0) ! CHECK: i = ifix(1)
  i = IDINT(4.25D1) ! CHECK: i = idint(42.5)
  r = INT(22) ! CHECK: r = real(int(22))

  i = INT() ! expected-error {{too few arguments to intrinsic function call, expected 1 or 2, have 0}}
  i = INT(1,2,3) ! expected-error {{too many arguments to intrinsic function call, expected 1 or 2, have 3}}

  i = IFIX(22) ! expected-error {{passing 'integer' to parameter of incompatible type 'real'}}
  i = idint(22) ! expected-error {{passing 'integer' to parameter of incompatible type 'double precision'}}
  i = int(.true.) ! expected-error {{passing 'logical' to parameter of incompatible type 'integer' or 'real' or 'complex'}}

  r = REAL(42) ! CHECK: r = real(42)
  r = real(1D1) ! CHECK: r = real(10)
  r = float(13) ! CHECK: r = float(13)
  r = SNGL(0d0) ! CHECK: r = sngl(0)

  r = FLOAT(12.1) ! expected-error {{passing 'real' to parameter of incompatible type 'integer'}}
  r = sngl(12) ! expected-error {{passing 'integer' to parameter of incompatible type 'double precision'}}

  d = DBLE(i) ! CHECK: d = dble(i)
  d = DBLE(r) ! CHECK: d = dble(r)
  r = DBLE(i) ! CHECK: r = real(dble(i))
  r = real(c8) ! CHECK: r = real(real(c8))
  r8 = real(c8) ! CHECK: r8 = real(c8)

  c = cmplx(2.0)
  c = CMPLX(33)
  c = CMPLX(1,2)
  c = cmplx(1.0, i)
  c = cmplx(1, c) ! expected-error {{passing 'complex' to parameter of incompatible type 'integer' or 'real'}}
  c = cmplx( (1.0, 2.0 ) )
  c = CMPLX() ! expected-error {{too few arguments to intrinsic function call, expected 1 or 2, have 0}}
  c = CMPLX(1,2,3,4) ! expected-error {{too many arguments to intrinsic function call, expected 1 or 2, have 4}}
  c = CMPLX(1.0, .false.) ! expected-error {{passing 'logical' to parameter of incompatible type 'integer' or 'real'}}

  i = ICHAR('HELLO')
  i = ichar(.false.) ! expected-error {{passing 'logical' to parameter of incompatible type 'character'}}

  string = CHAR(65)
  string = char('TRUTH') ! expected-error {{passing 'character' to parameter of incompatible type 'integer'}}

  i64 = int(i,8)
  r8 = real(r,8)
  c8 = complex(c,8)
  i = int(i,1.0) ! expected-error {{passing 'real' to parameter 'kind' of incompatible type 'integer'}}
  i = int(i,22) ! expected-error {{invalid kind selector '22' for type 'integer'}}
  i = int(i,kind(i))

!! misc and maths functions

  r = AINT(r) ! CHECK: r = aint(r)
  d = AINT(d) ! CHECK: d = aint(d)
  d = DINT(d) ! CHECK: d = dint(d)
  d = DINT(r) ! expected-error {{passing 'real' to parameter of incompatible type 'double precision'}}

  r = ANINT(r) ! CHECK: r = anint(r)
  d = ANINT(d) ! CHECK: d = anint(d)
  d = DNINT(d) ! CHECK: d = dnint(d)
  d = DNINT(r) ! expected-error {{passing 'real' to parameter of incompatible type 'double precision'}}

  i = NINT(r) ! CHECK: i = nint(r)
  i = NINT(d) ! CHECK: i = nint(d)
  i = IDNINT(d) ! CHECK: i = idnint(d)
  i = IDNINT(r) ! expected-error {{passing 'real' to parameter of incompatible type 'double precision'}}

  i = ceiling(r) + floor(d) ! CHECK: i = (ceiling(r)+floor(d))

  i = ABS(i) ! CHECK: i = abs(i)
  r = ABS(r) ! CHECK: r = abs(r)
  d = ABS(d) ! CHECK: d = abs(d)
  r = ABS(c) ! CHECK: r = abs(c)
  i = IABS(i) ! CHECK: i = iabs(i)
  d = DABS(d) ! CHECK: d = dabs(d)
  r = CABS(c) ! CHECK: r = cabs(c)

  i = MOD(3,i)     ! CHECK: i = mod(3, i)
  r = MOD(r, 3.0)  ! CHECK: r = mod(r, 3)
  r = SIGN(r, 0.0) ! CHECK: r = sign(r, 0)
  d = DPROD(r, r)  ! CHECK: d = dprod(r, r)
  r = max(1.0, r)  ! CHECK: r = max(1, r)
  i = min(i, 11)

  i = mod(i64, 4) ! expected-error {{conflicting types in arguments 'x' and 'y' ('integer (Kind=8)' and 'integer')}}
  i = sign(i64, i) ! expected-error {{conflicting types in arguments 'x' and 'y' ('integer (Kind=8)' and 'integer')}}

  i = LEN(string) ! CHECK: i = len(string)
  i = len_trim(string) ! CHECK: i = len_trim(string)
  i = LEN(22) ! expected-error {{passing 'integer' to parameter of incompatible type 'character'}}
  i = INDEX(string, string) ! CHECK: i = index(string, string)

  r = aimag(c) ! CHECK: r = aimag(c)
  c = CONJG(c) ! CHECK: c = conjg(c)
  c = CONJG(r) ! expected-error {{passing 'real' to parameter of incompatible type 'complex'}}

  r = SQRT(r) ! CHECK: r = sqrt(r)
  d = SQRT(d) ! CHECK: d = sqrt(d)
  c = SQRT(c) ! CHECK: c = sqrt(c)

  r = EXP(r) ! CHECK: r = exp(r)
  d = EXP(d) ! CHECK: d = exp(d)
  c = EXP(c) ! CHECK: c = exp(c)

  r = SQRT(.false.) ! expected-error {{passing 'logical' to parameter of incompatible type 'real' or 'complex'}}

  r = LOG(r) ! CHECK: r = log(r)
  d = LOG(d) ! CHECK: d = log(d)
  c = LOG(c) ! CHECK: c = log(c)

  r = Log10(r) ! CHECK: r = log10(r)
  d = Log10(d) ! CHECK: d = log10(d)
  c = Log10(c) ! expected-error {{passing 'complex' to parameter of incompatible type 'real'}}

  r = SIN(r) ! CHECK: r = sin(r)
  d = SIN(d) ! CHECK: d = sin(d)
  c = SIN(c) ! CHECK: c = sin(c)

  r = TAN(r) ! CHECK: r = tan(r)
  d = TAN(d) ! CHECK: d = tan(d)

  r = ALOG10(r) ! CHECK: r = alog10(r)
  d = DLOG10(d) ! CHECK: d = dlog10(d)
  c = CLOG(c) ! CHECK: c = clog(c)

  r = ATAN2(r, 1.0) ! CHECK: r = atan2(r, 1)
  d = ATAN2(d, 1D0) ! CHECK: d = atan2(d, 1)
  r = atan2(r, i) ! expected-error {{conflicting types in arguments 'x' and 'y' ('real' and 'integer')}}

!! lexical comparison functions

  logicalResult = lge(string, string) ! CHECK: logicalresult = lge(string, string)
  logicalResult = lgt(string, string) ! CHECK: logicalresult = lgt(string, string)
  logicalResult = lle(string, string) ! CHECK: logicalresult = lle(string, string)
  logicalResult = llt(string, string) ! CHECK: logicalresult = llt(string, string)

END PROGRAM
