#!/bin/sh
for file in bin/*; do
	mv $file ${file}.exe
done
mv bin/7bgzf.so.exe bin/7bgzf.dll
