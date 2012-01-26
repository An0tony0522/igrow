rm -f ../bin/BSD/x86/igrow ../obj/BSD/x86/*.o
clang++ -m32 -static -O3 -DNDEBUG -std=gnu++11 -o ../obj/BSD/x86/thread_pool.o ../src/thread_pool.cpp -I/home/hjli/boost_1_48_0 -c
clang++ -m32 -static -O3 -DNDEBUG -std=gnu++11 -o ../obj/BSD/x86/operation.o ../src/operation.cpp -I/home/hjli/boost_1_48_0 -c
clang++ -m32 -static -O3 -DNDEBUG -std=gnu++11 -o ../obj/BSD/x86/ligand.o ../src/ligand.cpp -I/home/hjli/boost_1_48_0 -c
clang++ -m32 -static -O3 -DNDEBUG -std=gnu++11 -o ../obj/BSD/x86/main.o ../src/main.cpp -I/home/hjli/boost_1_48_0 -c
clang++ -m32 -static -O3 -DNDEBUG -o ../bin/BSD/x86/igrow ../obj/BSD/x86/thread_pool.o ../obj/BSD/x86/operation.o ../obj/BSD/x86/ligand.o ../obj/BSD/x86/main.o -L/home/hjli/boost_1_48_0/lib/x86 -pthread -lboost_system -lboost_thread -lboost_filesystem -lboost_program_options