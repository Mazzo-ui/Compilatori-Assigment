int test_licm(int n, int a, int b) {
    int result = 0;
    int c = a + b;

    for (int i = 0; i < n; i++) {
        int x = a * b;      // loop-invariant: a e b sono definiti fuori dal loop
        int y = x + c;      // loop-invariant: x e c non cambiano nel loop
        result += y + i;    // non loop-invariant: dipende da i e da result
    }

    return result;
}

int test_branch(int n, int a, int b, int flag) {
    int result = 0;

    for (int i = 0; i < n; i++) {
        int x = a + b;      // loop-invariant e nel blocco principale del loop
        if (flag) {
            result += x;
        } else {
            result += i;
        }
    }

    return result;
}
