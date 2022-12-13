hp:
	@echo " Compile hp_main ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/hp_main.c ./src/record.c ./src/hp_file.c -lbf -o ./build/hp_main

bf:
	@echo " Compile bf_main ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/bf_main.c ./src/record.c -lbf -o ./build/bf_main

ht:
	@echo " Compile hp_main ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/ht_main.c ./src/record.c ./src/ht_table.c -lbf -o ./build/ht_main

clean:
	rm ./build/hp_main data.db block_example.db

