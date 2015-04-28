! RUN: %flang -triple "x86_64-unknown-linux" -emit-llvm -o - %s | %file_check %s

complex function foo() ! CHECK: define <2 x float> @foo_()
  foo = (1.0, 2.0)
end

complex(8) function bar() ! CHECK: define { double, double } @bar_()
  bar = 0.0
end
