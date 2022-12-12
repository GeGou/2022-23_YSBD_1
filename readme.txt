Για την δημιουργία του εκτελέσιμου το οποίο δείχνει
τις δυνατότητες της βιβλιοθήκης BF, τρέξτε την εντολή:

make bf;

Για να τρέξετε το εκτελέσιμο:

./build/bf_main

Αντίστοιχα και για τα άλλα εκτελέσιμα.
make ht;
make hp;

Έλενχος για leaks
valgrind --leak-check=full --show-leak-kinds=all --trace-children=yes ./build/bf_main
