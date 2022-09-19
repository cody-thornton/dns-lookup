multi-lookup : multi-lookup.o util.o
	gcc -o multi-lookup multi-lookup.o util.o -lpthread

multi-lookup.o : multi-lookup.c  util.h
	gcc -Wall -c multi-lookup.c

util.o : util.c util.h
	gcc -Wall -c util.c

clean :
	rm *.o multi-lookup

test :
	./multi-lookup 5 3 parsingLog1.txt converterLog1.txt ./input/names1.txt ./input/names2.txt ./input/names3.txt ./input/names4.txt ./input/names5.txt

testC :
	./multi-lookup 5 10 parsingLogC.txt converterLogC.txt ./C/*.txt

testD :
	./multi-lookup 5 10 parsingLogD.txt converterLogD.txt ./D/*.txt

testE :
	./multi-lookup 5 10 parsingLogE.txt converterLogE.txt ./E/*.txt
