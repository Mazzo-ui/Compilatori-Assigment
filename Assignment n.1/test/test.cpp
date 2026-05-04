

int test_algebraic_identity(int x, int y) {
    int a = x + 0;
    int b = 0 + y;
    int c = x * 1;
    int d = 1 * y;
    return a + b + c + d;
}

int test_strength_reduction(int x) {
    int a = x * 2;    // 2^1
    int b = x * 3;    // 2^2 - 1
    int c = x * 5;    // 2^2 + 1
    int d = x * 8;    // 2^3
    int e = x * 15;   // 2^4 - 1
    int f = x * 17;   // 2^4 + 1
    int g = x * 63;   // 2^6 - 1
    int h = x * 64;   // 2^6
    int i = x * 65;   // 2^6 + 1

    int j = x / 2;
    int k = x / 4;
    int l = x / 8;
    int m = x / 16;

    return a + b + c + d + e + f + g + h + i + j + k + l + m;
}

int test_multi_instruction_1(int B) {
    int t = B + 1;
    int r = t - 1;
    return r;
}

int test_multi_instruction_2(int B) {
    int t = B - 1;
    int r = t + 1;
    return r;
}

int test_multi_instruction_3(int B) {
    int t = B - 1;
    int r = 1 + t;
    return r;
}

int test_multi_instruction_4(int B) {
    int t = B + 1;
    int r = 1 - t;
    return r;
}

int test_multi_instruction_5(int B) {
    int t = B + 5;
    int r = t - 5;
    return r;
}

int test_multi_instruction_6(int B) {
    int t = B - 7;
    int r = t + 7;
    return r;
}

int test_multi_instruction_7(int B) {
    int t = B - 9;
    int r = 9 + t;
    return r;
}

int test_multi_instruction_8(int B) {
    int t = B + 12;
    int r = 12 - t;
    return r;
}

int main() {
    volatile int x = 10;
    volatile int y = 20;
    volatile int B = 30;

    int r1 = test_algebraic_identity(x, y);
    int r2 = test_strength_reduction(x);
    int r3 = test_multi_instruction_1(B);
    int r4 = test_multi_instruction_2(B);
    int r5 = test_multi_instruction_3(B);
    int r6 = test_multi_instruction_4(B);
    int r7 = test_multi_instruction_5(B);
    int r8 = test_multi_instruction_6(B);
    int r9 = test_multi_instruction_7(B);
    int r10 = test_multi_instruction_8(B);

    return r1 + r2 + r3 + r4 + r5 + r6 + r7 + r8 + r9 + r10;
}

