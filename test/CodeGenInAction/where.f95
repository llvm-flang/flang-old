! RUN: %flang -interpret %s | %file_check %s

program wheretest
  integer i_mat(3,3), i_mat2(3,3)
  logical l_mat(3,3)

  data i_mat / 1, -1, 0, -11, 1, 0, -43, 0, 1 /

  i_mat2 = 0

  where(i_mat <= i_mat2)
    i_mat = i_mat2
  else where
    i_mat = 42
  end where

  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK: 42, 0, 0, 0, 42, 0, 0, 0, 42

end
