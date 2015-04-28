! RUN: %flang -emit-llvm -o - %s

PROGRAM test

  integer i_mat(4,4), i_mat2(4,4)
  integer i

  i_mat = 0
  i_mat2 = 1
  i_mat2(1,2) = 0
  i_mat2(3,3) = 0

  where(i_mat < i_mat2)
    i_mat = i_mat2
  else where
    i_mat = -24
  end where

END
