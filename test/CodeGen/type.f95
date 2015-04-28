! RUN: %flang -emit-llvm -o - %s | %file_check %s

program typeTest

  type Point
    sequence
    real x,y
  end type

  type Triangle
    type(Point) vertices(3)
    integer color
  end type

  type(Point) p    ! CHECK: alloca { float, float }
  type(Triangle) t ! CHECK: alloca { [3 x { float, float }], i32 }
  type(Point) pa(3)

  type(Point) gen
  external gen


  p = Point(1.0,0.0)
  p = p ! CHECK: store { float, float } {{.*}}, { float, float }*
  pa(1) = p
  p = pa(1)

  p%x = 1.0
  p%y = p%x
  pa(1)%x = p%y

  ! FIXME: t%vertices(1) = p
  t%color = 0

  p = gen()

end program

subroutine sub(p) ! CHECK: define void @sub_({ float, float }* noalias
  type Point
    sequence
    real x,y
  end type
  type(Point) p
  p%x= 1.0
end

function func(i) ! CHECK: define {{.*}} @func_(i32* noalias
  integer i
  type Point
    sequence
    real x,y
  end type
  type(Point) func
  func = Point(i, 4.25)
end

subroutine foo

  type Point
    real x,y
  end type

  type(Point) p1, p2

  data p1 / Point(1, 2) /      ! CHECK: store { float, float }
  data p2%x, p2%y / 2.0, 4.0 / ! CHECK: store { float, float }

end
