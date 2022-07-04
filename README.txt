-----------------------------------
To build on Linux PC/VirtualMachine
-----------------------------------
$$ libtoolize --force
$$ aclocal
$$ autoheader
$$ automake --force-missing --add-missing
$$ autoconf
$$ PREFIX_PATH=/ZZZZZZ/local
$$ CPPFLAGS="-I$PREFIX_PATH/include/ -I$PREFIX_PATH/include/cjson/" LIBCJSON_CFLAGS=-I$PREFIX_PATH/include/cjson/ LDFLAGS="-L$PREFIX_PATH/lib/ -lbtrCore" LIBCJSON_LIBS=-lcjson ./configure  --enable-gstreamer1=yes --enable-pi-build=yes --prefix=$PREFIX_PATH

