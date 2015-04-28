! RUN: %flang -emit-llvm -o - %s

PROGRAM test

  integer i_mat(4,4), i_mat2(4,4)
  integer i

  i = 11
  i_mat = i
  i_mat2 = i_mat
  i_mat(1:3,:) = 2
  i_mat(:1,:1) = 11
  i_mat(1,:) = 10
  i_mat(:2,4) = 12
  i_mat(:3:2,1:3:1) = 13

END
