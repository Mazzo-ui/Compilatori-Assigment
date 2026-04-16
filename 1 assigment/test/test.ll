; ModuleID = '../test/test.ll'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

;int test_algebraic_identity(int x, int y) {
;    int a = x + 0;
;    int b = 0 + y;
;    int c = x * 1;
;    int d = 1 * y;
;    return a + b + c + d;
;}
; Function Attrs: mustprogress noinline nounwind uwtable
define dso_local noundef i32 @_Z23test_algebraic_identityii(i32 noundef %0, i32 noundef %1) #0 {
  %3 = add nsw i32 %0, 0
  %4 = add nsw i32 0, %1
  %5 = mul nsw i32 %0, 1
  %6 = mul nsw i32 1, %1
  %7 = add nsw i32 %3, %4
  %8 = add nsw i32 %7, %5
  %9 = add nsw i32 %8, %6
  ret i32 %9
}

;int test_strength_reduction(int x) {
;    int a = x * 2;    
;    int b = x * 3;    
;    int c = x * 5;    
;    int d = x * 8;    
;    int e = x * 15;   
;    int f = x * 17;   
;    int g = x * 63;   
;    int h = x * 64;   
;    int i = x * 65;   
;
;    int j = x / 2;
;    int k = x / 4;
;    int l = x / 8;
;    int m = x / 16;
;
;    return a + b + c + d + e + f + g + h + i + j + k + l + m;
;}
; Function Attrs: mustprogress noinline nounwind uwtable
define dso_local noundef i32 @_Z23test_strength_reductioni(i32 noundef %0) #0 {
  %2 = mul nsw i32 %0, 2
  %3 = mul nsw i32 %0, 3
  %4 = mul nsw i32 %0, 5
  %5 = mul nsw i32 %0, 8
  %6 = mul nsw i32 %0, 15
  %7 = mul nsw i32 %0, 17
  %8 = mul nsw i32 %0, 63
  %9 = mul nsw i32 %0, 64
  %10 = mul nsw i32 %0, 65
  %11 = sdiv i32 %0, 2
  %12 = sdiv i32 %0, 4
  %13 = sdiv i32 %0, 8
  %14 = sdiv i32 %0, 16
  %15 = add nsw i32 %2, %3
  %16 = add nsw i32 %15, %4
  %17 = add nsw i32 %16, %5
  %18 = add nsw i32 %17, %6
  %19 = add nsw i32 %18, %7
  %20 = add nsw i32 %19, %8
  %21 = add nsw i32 %20, %9
  %22 = add nsw i32 %21, %10
  %23 = add nsw i32 %22, %11
  %24 = add nsw i32 %23, %12
  %25 = add nsw i32 %24, %13
  %26 = add nsw i32 %25, %14
  ret i32 %26
}

;int test_multi_instruction_1(int B) {
;    int t = B + 1;
;    int r = t - 1;
;    return r;
;}
; Function Attrs: mustprogress noinline nounwind uwtable
define dso_local noundef i32 @_Z24test_multi_instruction_1i(i32 noundef %0) #0 {
  %2 = add nsw i32 %0, 1
  %3 = sub nsw i32 %2, 1
  ret i32 %3
}

;int test_multi_instruction_2(int B) {
;    int t = B - 1;
;    int r = t + 1;
;    return r;
;}
; Function Attrs: mustprogress noinline nounwind uwtable
define dso_local noundef i32 @_Z24test_multi_instruction_2i(i32 noundef %0) #0 {
  %2 = sub nsw i32 %0, 1
  %3 = add nsw i32 %2, 1
  ret i32 %3
}

;int test_multi_instruction_3(int B) {
;    int t = B - 1;
;    int r = 1 + t;
;    return r;
;}
; Function Attrs: mustprogress noinline nounwind uwtable
define dso_local noundef i32 @_Z24test_multi_instruction_3i(i32 noundef %0) #0 {
  %2 = sub nsw i32 %0, 1
  %3 = add nsw i32 1, %2
  ret i32 %3
}

;int test_multi_instruction_4(int B) {
;    int t = B + 1;
;    int r = 1 - t;
;    return r;
;}
; Function Attrs: mustprogress noinline nounwind uwtable
define dso_local noundef i32 @_Z24test_multi_instruction_4i(i32 noundef %0) #0 {
  %2 = add nsw i32 %0, 1
  %3 = sub nsw i32 1, %2
  ret i32 %3
}

;int test_multi_instruction_5(int B) {
;    int t = B + 5;
;    int r = t - 5;
;    return r;
;}
; Function Attrs: mustprogress noinline nounwind uwtable
define dso_local noundef i32 @_Z24test_multi_instruction_5i(i32 noundef %0) #0 {
  %2 = add nsw i32 %0, 5
  %3 = sub nsw i32 %2, 5
  ret i32 %3
}

;int test_multi_instruction_6(int B) {
;    int t = B - 7;
;    int r = t + 7;
;    return r;
;}
; Function Attrs: mustprogress noinline nounwind uwtable
define dso_local noundef i32 @_Z24test_multi_instruction_6i(i32 noundef %0) #0 {
  %2 = sub nsw i32 %0, 7
  %3 = add nsw i32 %2, 7
  ret i32 %3
}

;int test_multi_instruction_7(int B) {
;    int t = B - 9;
;    int r = 9 + t;
;    return r;
;}
; Function Attrs: mustprogress noinline nounwind uwtable
define dso_local noundef i32 @_Z24test_multi_instruction_7i(i32 noundef %0) #0 {
  %2 = sub nsw i32 %0, 9
  %3 = add nsw i32 9, %2
  ret i32 %3
}

;int test_multi_instruction_8(int B) {
;    int t = B + 12;
;    int r = 12 - t;
;    return r;
;}
; Function Attrs: mustprogress noinline nounwind uwtable
define dso_local noundef i32 @_Z24test_multi_instruction_8i(i32 noundef %0) #0 {
  %2 = add nsw i32 %0, 12
  %3 = sub nsw i32 12, %2
  ret i32 %3
}

;int main() {
;    volatile int x = 10;
;    volatile int y = 20;
;    volatile int B = 30;
;
;    int r1 = test_algebraic_identity(x, y);
;    int r2 = test_strength_reduction(x);
;    int r3 = test_multi_instruction_1(B);
;    int r4 = test_multi_instruction_2(B);
;    int r5 = test_multi_instruction_3(B);
;    int r6 = test_multi_instruction_4(B);
;    int r7 = test_multi_instruction_5(B);
;    int r8 = test_multi_instruction_6(B);
;    int r9 = test_multi_instruction_7(B);
;    int r10 = test_multi_instruction_8(B);
;
;    return r1 + r2 + r3 + r4 + r5 + r6 + r7 + r8 + r9 + r10;
;}
; Function Attrs: mustprogress noinline norecurse nounwind uwtable
define dso_local noundef i32 @main() #1 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  store volatile i32 10, ptr %1, align 4
  store volatile i32 20, ptr %2, align 4
  store volatile i32 30, ptr %3, align 4
  %4 = load volatile i32, ptr %1, align 4
  %5 = load volatile i32, ptr %2, align 4
  %6 = call noundef i32 @_Z23test_algebraic_identityii(i32 noundef %4, i32 noundef %5)
  %7 = load volatile i32, ptr %1, align 4
  %8 = call noundef i32 @_Z23test_strength_reductioni(i32 noundef %7)
  %9 = load volatile i32, ptr %3, align 4
  %10 = call noundef i32 @_Z24test_multi_instruction_1i(i32 noundef %9)
  %11 = load volatile i32, ptr %3, align 4
  %12 = call noundef i32 @_Z24test_multi_instruction_2i(i32 noundef %11)
  %13 = load volatile i32, ptr %3, align 4
  %14 = call noundef i32 @_Z24test_multi_instruction_3i(i32 noundef %13)
  %15 = load volatile i32, ptr %3, align 4
  %16 = call noundef i32 @_Z24test_multi_instruction_4i(i32 noundef %15)
  %17 = load volatile i32, ptr %3, align 4
  %18 = call noundef i32 @_Z24test_multi_instruction_5i(i32 noundef %17)
  %19 = load volatile i32, ptr %3, align 4
  %20 = call noundef i32 @_Z24test_multi_instruction_6i(i32 noundef %19)
  %21 = load volatile i32, ptr %3, align 4
  %22 = call noundef i32 @_Z24test_multi_instruction_7i(i32 noundef %21)
  %23 = load volatile i32, ptr %3, align 4
  %24 = call noundef i32 @_Z24test_multi_instruction_8i(i32 noundef %23)
  %25 = add nsw i32 %6, %8
  %26 = add nsw i32 %25, %10
  %27 = add nsw i32 %26, %12
  %28 = add nsw i32 %27, %14
  %29 = add nsw i32 %28, %16
  %30 = add nsw i32 %29, %18
  %31 = add nsw i32 %30, %20
  %32 = add nsw i32 %31, %22
  %33 = add nsw i32 %32, %24
  ret i32 %33
}

attributes #0 = { mustprogress noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { mustprogress noinline norecurse nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"clang version 19.1.7 (/home/runner/work/llvm-project/llvm-project/clang cd708029e0b2869e80abe31ddb175f7c35361f90)"}
