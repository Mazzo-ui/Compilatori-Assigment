int main() {
  int arr[100];

  for (int i = 0; i < 100; i++) {
    arr[i] = 10;
    //spostiamo nel preheader solo il calcolo del puntatore arr[0] poichè = 1 ha un side effect essendo
    //un'operazione di store
    arr[0] = 1;
    int c = arr[i] + 2;
  }
}
