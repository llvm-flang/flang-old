! RUN: %flang -interpret %s | %file_check %s

function swap(p)
  type Point
    sequence
    integer x,y
  end type

  type(Point) swap, p
  swap%x = p%y
  swap%y = p%x
end

program typeTest

  type Point
    sequence
    integer x,y
  end type

  type Triangle
    type(Point) vertices(3)
    integer color
  end type

  type(Point) p
  type(Triangle) t
  type(Point) pa(3)

  print *, 'START' ! CHECK: START
  p = Point(1,2)
  print *, p%x, ', ', p%y ! CHECK: 1, 2
  p%x = 4
  print *, p%x, ', ', p%y ! CHECK: 4, 2
  p%y = p%x
  print *, p%x, ', ', p%y ! CHECK: 4, 4

  pa(1) = p
  print *, pa(1)%x, ', ', pa(1)%y ! CHECK: 4, 4
  pa(2) = Point(42,31)
  print *, pa(2)%x, ', ', pa(2)%y ! CHECK: 42, 31
  pa(3) = pa(1)
  print *, pa(3)%x, ', ', pa(3)%y ! CHECK: 4, 4

  p = swap(Point(9,0))
  print *, p%x, ', ', p%y ! CHECK: 0, 9

end program
