

void fusion_ok(int *A, int *B, int *C, int n) {
  for (int i = 0; i < n; i++) {
    A[i] = B[i] + 1;
  }

  for (int j = 0; j < n; j++) {
    C[j] = A[j] + 2;
  }
}

void fusion_no_different_trip_count(int *A, int *B, int *C, int n) {
  for (int i = 0; i < n; i++) {
    A[i] = B[i] + 1;
  }

  for (int j = 0; j < n + 1; j++) {
    C[j] = B[j] + 2;
  }
}

void fusion_no_negative_dependence(int *A, int *B, int n) {
  for (int i = 0; i < n; i++) {
    A[i] = B[i] + 1;
  }

  // Questo schema rappresenta il caso problematico:
  // il secondo loop legge A[i+3], cioe' un valore prodotto dal primo loop
  // a una iterazione futura rispetto alla fusione.
  for (int j = 0; j < n - 3; j++) {
    B[j] = A[j + 3] + 1;
  }
}
