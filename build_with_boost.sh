mkdir third_party_source
cd third_party_source

wget https://sourceforge.net/projects/boost/files/boost/1.67.0/boost_1_67_0.tar.gz
tar zxvf boost_1_67_0.tar.gz
mv boost_1_67_0 boost
cd boost

./bootstrap.sh --prefix=$PWD/../../third_party/boost/ \
    --with-libraries=iostreams,date_time,thread,system \
    --without-icu --without-icu
./b2 install -j8 --disable-icu --ignore-site-config "cxxflags=-std=c++17 -fPIC" \
    link=static threading=multi runtime-link=static

cd ../..
mkdir build
cd build

cmake -DBOOST_ROOT=$PWD/../third_party/boost/ \
    -DCMAKE_INSTALL_PREFIX=$PWD/../target/nanorpc ..
make install
