; RUN: opt -load-pass-plugin %llvmshlibdir/LlvmFmaPass_Opolin_Dmitry_FIIT2_LLVM_IR%pluginext\
; RUN: -passes=LlvmFmaPass -S %s | FileCheck %s

; CHECK-LABEL: @basic_case
; CHECK: call double @llvm.fmuladd.f64(double %a, double %b, double %c)
; CHECK-NOT: fmul double
; CHECK-NOT: fadd double
define double @basic_case(double %a, double %b, double %c) {
  %mul = fmul double %a, %b
  %add = fadd double %mul, %c
  ret double %add
}

; CHECK-LABEL: @reverse_order
; CHECK: call double @llvm.fmuladd.f64(double %a, double %b, double %c)
define double @reverse_order(double %a, double %b, double %c) {
  %mul = fmul double %a, %b
  %add = fadd double %c, %mul
  ret double %add
}

; CHECK-LABEL: @with_constants
; CHECK: call double @llvm.fmuladd.f64(double %a, double 2.0{{[0+e+]*}}, double 3.0{{[0+e+]*}})
define double @with_constants(double %a) {
  %mul = fmul double %a, 2.0
  %add = fadd double %mul, 3.0
  ret double %add
}

; CHECK-LABEL: @negative_constant
; CHECK: call double @llvm.fmuladd.f64(double %a, double %b, double -1.0{{[0+e+]*}})
define double @negative_constant(double %a, double %b) {
  %mul = fmul double %a, %b
  %add = fadd double %mul, -1.0
  ret double %add
}

; CHECK-LABEL: @float_type
; CHECK: call float @llvm.fmuladd.f32(float %a, float %b, float %c)
define float @float_type(float %a, float %b, float %c) {
  %mul = fmul float %a, %b
  %add = fadd float %mul, %c
  ret float %add
}

; CHECK-LABEL: @multi_use
; CHECK-DAG: call double @llvm.fmuladd.f64(double %a, double %b, double %c)
; CHECK-DAG: call double @llvm.fmuladd.f64(double %a, double %b, double %d)
; CHECK-NOT: fmul double
; CHECK-NOT: fadd double
define double @multi_use(double %a, double %b, double %c, double %d) {
  %mul = fmul double %a, %b
  %add1 = fadd double %mul, %c
  %add2 = fadd double %mul, %d
  ret double %add2
}

; CHECK-LABEL: @int_ops
; CHECK: mul i32 %a, %b
; CHECK: add i32 %mul, %c
define i32 @int_ops(i32 %a, i32 %b, i32 %c) {
  %mul = mul i32 %a, %b
  %add = add i32 %mul, %c
  ret i32 %add
}

; CHECK-LABEL: @mixed_types
; CHECK: call double @llvm.fmuladd.f64(double %a, double %b_ext, double %c)
; CHECK-NOT: fmul double
; CHECK-NOT: fadd double
define double @mixed_types(double %a, float %b, double %c) {
  %b_ext = fpext float %b to double
  %mul = fmul double %a, %b_ext
  %add = fadd double %mul, %c
  ret double %add
}

; CHECK-LABEL: @no_change
; CHECK: fadd double %a, %b
; CHECK-NOT: fmuladd
define double @no_change(double %a, double %b) {
  %add = fadd double %a, %b
  ret double %add
}
