if [ $# -ne 2 ]; then
	echo "Missing input file and algorithm !"
	exit 1
	fi

echo "Compressing..."
Release/compress -cvs $1 -m $2 test_out.bin > test.txt

echo "Expanding..."
Release/compress -ev -m $2 test_out.bin test_in.bin >> test.txt

echo "Comparing..."
dump $1 > $1.txt
dump test_in.bin > test_in.txt
diff --color $1.txt test_in.txt
