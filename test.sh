echo "Compressing data..."
Release/compress -sc data.bin > data.txt
diff --color data.0.txt data.txt
echo ""

echo "Compressing code..."
Release/compress -sc code.bin > code.txt
diff --color code.0.txt code.txt
echo ""

echo "Compressing ash..."
Release/compress -sc ash.bin > ash.txt
diff --color ash.0.txt ash.txt
echo ""
