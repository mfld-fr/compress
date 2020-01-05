if [ $# -ne 2 ]; then
	echo "Missing input file and algorithm !"
	exit 1
	fi

echo "1- COMPRESS"
Release/compress -cv $1 -m $2 test_out.bin | tee test.txt

echo "2- EXPAND"
Release/compress -ev -m $2 test_out.bin test_in.bin | tee -a test.txt

echo "3- COMPARE"
dump $1 > $1.txt
dump test_in.bin > test_in.txt
diff --color $1.txt test_in.txt
