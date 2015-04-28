! RUN: %flang -emit-llvm -o - %s

PROGRAM test

  integer i_mat(4,4), i_mat2(4,4)
  real    r_mat(4,4)
  complex c_mat(4,4), c_mat2(4,4)
  logical l_mat(4,4)
  integer i

  i = 11
  i_mat = 11
  i_mat2 = 12
  i_mat = 1.0
  r_mat = 1.0
  c_mat = (1.0,0.0)
  c_mat2 = c_mat
  i_mat = r_mat

  i_mat = i_mat + i_mat2
  i_mat2 = i_mat * (2 - 4) + r_mat

  c_mat = c_mat * c_mat2
  i_mat = c_mat + i_mat2

  l_mat = i_mat <= i_mat2

  i_mat = int(r_mat)
  r_mat = real(i_mat)
  c_mat = cmplx(c_mat2)
  c_mat = cmplx(i_mat,r_mat)

  r_mat = aimag(c_mat)
  c_mat = conjg(c_mat)
  i_mat = abs(i_mat) + sqrt(r_mat)
  r_mat = sin(r_mat) * cos(r_mat) + tan(r_mat)
  c_mat = exp(c_mat) + sin(c_mat)

END
